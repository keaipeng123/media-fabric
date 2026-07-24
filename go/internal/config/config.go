package config

import (
	"bufio"
	"errors"
	"os"
	"strconv"
	"strings"
)

type API struct {
	Enabled                                                                     bool
	ListenAddress, DatabaseURL, JWTSecret, BootstrapUsername, BootstrapPassword string
	AccessTTLMinutes, RefreshTTLMinutes                                         int
}

func LoadAPI(path string) (API, error) {
	c := API{ListenAddress: "127.0.0.1:8080", AccessTTLMinutes: 15, RefreshTTLMinutes: 10080}
	f, err := os.Open(path)
	if err != nil {
		return c, err
	}
	defer f.Close()
	section := ""
	s := bufio.NewScanner(f)
	for s.Scan() {
		line := strings.TrimSpace(s.Text())
		if line == "" || strings.HasPrefix(line, "#") || strings.HasPrefix(line, ";") {
			continue
		}
		if strings.HasPrefix(line, "[") && strings.HasSuffix(line, "]") {
			section = strings.ToLower(strings.TrimSpace(line[1 : len(line)-1]))
			continue
		}
		if section != "api" {
			continue
		}
		parts := strings.SplitN(line, "=", 2)
		if len(parts) != 2 {
			continue
		}
		key, value := strings.TrimSpace(parts[0]), strings.TrimSpace(parts[1])
		switch key {
		case "enabled":
			c.Enabled = strings.EqualFold(value, "true") || value == "1"
		case "listen_address":
			c.ListenAddress = value
		case "database_url":
			c.DatabaseURL = value
		case "jwt_secret":
			c.JWTSecret = value
		case "bootstrap_admin_username":
			c.BootstrapUsername = value
		case "bootstrap_admin_password":
			c.BootstrapPassword = value
		case "access_token_ttl_minutes":
			c.AccessTTLMinutes, _ = strconv.Atoi(value)
		case "refresh_token_ttl_minutes":
			c.RefreshTTLMinutes, _ = strconv.Atoi(value)
		}
	}
	if err := s.Err(); err != nil {
		return c, err
	}
	for _, env := range []struct {
		k string
		p *string
	}{{"MEDIA_FABRIC_DATABASE_URL", &c.DatabaseURL}, {"MEDIA_FABRIC_JWT_SECRET", &c.JWTSecret}, {"MEDIA_FABRIC_BOOTSTRAP_ADMIN_PASSWORD", &c.BootstrapPassword}} {
		if v := os.Getenv(env.k); v != "" {
			*env.p = v
		}
	}
	if !c.Enabled {
		return c, nil
	}
	if c.DatabaseURL == "" || c.JWTSecret == "" || c.BootstrapUsername == "" || c.BootstrapPassword == "" {
		return c, errors.New("[api] requires database_url, jwt_secret, bootstrap_admin_username and bootstrap_admin_password")
	}
	if len(c.JWTSecret) < 32 {
		return c, errors.New("api jwt_secret must contain at least 32 characters")
	}
	return c, nil
}
