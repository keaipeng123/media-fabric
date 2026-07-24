package auth

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
)

const iterations = 600000

func derive(password string, salt []byte) []byte {
	data := append(append([]byte{}, salt...), 0, 0, 0, 1)
	h := hmac.New(sha256.New, []byte(password))
	h.Write(data)
	u := h.Sum(nil)
	out := append([]byte{}, u...)
	for i := 1; i < iterations; i++ {
		h = hmac.New(sha256.New, []byte(password))
		h.Write(u)
		u = h.Sum(nil)
		for j := range out {
			out[j] ^= u[j]
		}
	}
	return out
}
func HashPassword(p string) (string, error) {
	s := make([]byte, 16)
	if _, e := rand.Read(s); e != nil {
		return "", e
	}
	return fmt.Sprintf("pbkdf2-sha256$%d$%s$%s", iterations, base64.RawStdEncoding.EncodeToString(s), base64.RawStdEncoding.EncodeToString(derive(p, s))), nil
}
func VerifyPassword(p, encoded string) bool {
	v := strings.Split(encoded, "$")
	if len(v) != 4 || v[0] != "pbkdf2-sha256" {
		return false
	}
	s, e := base64.RawStdEncoding.DecodeString(v[2])
	if e != nil {
		return false
	}
	got, e := base64.RawStdEncoding.DecodeString(v[3])
	return e == nil && subtle.ConstantTimeCompare(got, derive(p, s)) == 1
}

type Claims struct {
	Subject   string `json:"sub"`
	Role      string `json:"role"`
	Kind      string `json:"kind"`
	ExpiresAt int64  `json:"exp"`
}

func Sign(secret string, c Claims) (string, error) {
	h := base64.RawURLEncoding.EncodeToString([]byte(`{"alg":"HS256","typ":"JWT"}`))
	p, e := json.Marshal(c)
	if e != nil {
		return "", e
	}
	body := h + "." + base64.RawURLEncoding.EncodeToString(p)
	m := hmac.New(sha256.New, []byte(secret))
	m.Write([]byte(body))
	return body + "." + base64.RawURLEncoding.EncodeToString(m.Sum(nil)), nil
}
func Verify(secret, token, kind string) (Claims, error) {
	var c Claims
	v := strings.Split(token, ".")
	if len(v) != 3 {
		return c, errors.New("invalid token")
	}
	m := hmac.New(sha256.New, []byte(secret))
	m.Write([]byte(v[0] + "." + v[1]))
	sig, e := base64.RawURLEncoding.DecodeString(v[2])
	if e != nil || !hmac.Equal(sig, m.Sum(nil)) {
		return c, errors.New("invalid token signature")
	}
	p, e := base64.RawURLEncoding.DecodeString(v[1])
	if e != nil || json.Unmarshal(p, &c) != nil || c.Kind != kind || c.ExpiresAt <= time.Now().Unix() {
		return c, errors.New("expired or invalid token")
	}
	return c, nil
}
func RandomToken() (string, error) {
	b := make([]byte, 32)
	_, e := rand.Read(b)
	return base64.RawURLEncoding.EncodeToString(b), e
}
