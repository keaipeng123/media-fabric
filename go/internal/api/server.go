package api

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"github.com/media-fabric/media-fabric/internal/auth"
	"github.com/media-fabric/media-fabric/internal/config"
	"github.com/media-fabric/media-fabric/internal/store"
	"net/http"
	"strconv"
	"strings"
	"time"
)

type Server struct {
	cfg   config.API
	store *store.Store
}
type ctxKey struct{}

func New(cfg config.API, s *store.Store) *Server { return &Server{cfg, s} }
func (s *Server) Handler() http.Handler {
	m := http.NewServeMux()
	m.HandleFunc("/api/v1/health", s.health)
	m.HandleFunc("/api/v1/auth/login", s.login)
	m.HandleFunc("/api/v1/auth/refresh", s.refresh)
	m.HandleFunc("/api/v1/auth/logout", s.auth(s.logout))
	m.HandleFunc("/api/v1/me", s.auth(s.me))
	m.HandleFunc("/api/v1/users", s.auth(s.users))
	m.HandleFunc("/api/v1/users/", s.auth(s.user))
	return m
}
func write(w http.ResponseWriter, status int, v interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(v)
}
func decode(r *http.Request, v interface{}) error {
	d := json.NewDecoder(r.Body)
	d.DisallowUnknownFields()
	return d.Decode(v)
}
func (s *Server) health(w http.ResponseWriter, r *http.Request) {
	if r.Method != "GET" {
		write(w, 405, map[string]string{"error": "method not allowed"})
		return
	}
	write(w, 200, map[string]string{"status": "ok"})
}
func (s *Server) auth(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		v := strings.TrimPrefix(r.Header.Get("Authorization"), "Bearer ")
		if v == "" {
			write(w, 401, map[string]string{"error": "missing bearer token"})
			return
		}
		c, e := auth.Verify(s.cfg.JWTSecret, v, "access")
		if e != nil {
			write(w, 401, map[string]string{"error": "invalid access token"})
			return
		}
		id, e := strconv.ParseInt(c.Subject, 10, 64)
		if e != nil {
			write(w, 401, map[string]string{"error": "invalid access token"})
			return
		}
		u, e := s.store.Find(r.Context(), id)
		if e != nil || !u.Active {
			write(w, 401, map[string]string{"error": "account unavailable"})
			return
		}
		next(w, r.WithContext(context.WithValue(r.Context(), ctxKey{}, u)))
	}
}
func userFrom(r *http.Request) store.User { return r.Context().Value(ctxKey{}).(store.User) }
func (s *Server) tokens(ctx context.Context, u store.User) (map[string]interface{}, error) {
	now := time.Now()
	a, e := auth.Sign(s.cfg.JWTSecret, auth.Claims{Subject: strconv.FormatInt(u.ID, 10), Role: u.Role, Kind: "access", ExpiresAt: now.Add(time.Duration(s.cfg.AccessTTLMinutes) * time.Minute).Unix()})
	if e != nil {
		return nil, e
	}
	rt, e := auth.RandomToken()
	if e != nil {
		return nil, e
	}
	if e = s.store.SaveRefresh(ctx, u.ID, rt, now.Add(time.Duration(s.cfg.RefreshTTLMinutes)*time.Minute)); e != nil {
		return nil, e
	}
	return map[string]interface{}{"access_token": a, "refresh_token": rt, "token_type": "Bearer", "expires_in": s.cfg.AccessTTLMinutes * 60}, nil
}
func (s *Server) login(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		write(w, 405, map[string]string{"error": "method not allowed"})
		return
	}
	var q struct{ Username, Password string }
	if decode(r, &q) != nil || q.Username == "" || q.Password == "" {
		write(w, 400, map[string]string{"error": "username and password are required"})
		return
	}
	u, h, e := s.store.FindByUsername(r.Context(), q.Username)
	if e != nil || !u.Active || !auth.VerifyPassword(q.Password, h) {
		write(w, 401, map[string]string{"error": "invalid credentials"})
		return
	}
	out, e := s.tokens(r.Context(), u)
	if e != nil {
		write(w, 500, map[string]string{"error": "cannot create session"})
		return
	}
	s.store.Audit(r.Context(), u.ID, "auth.login", "user:"+u.Username, "")
	write(w, 200, out)
}
func (s *Server) refresh(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		write(w, 405, map[string]string{"error": "method not allowed"})
		return
	}
	var q struct {
		RefreshToken string `json:"refresh_token"`
	}
	if decode(r, &q) != nil || q.RefreshToken == "" {
		write(w, 400, map[string]string{"error": "refresh_token is required"})
		return
	}
	id, e := s.store.UseRefresh(r.Context(), q.RefreshToken)
	if e != nil {
		write(w, 401, map[string]string{"error": "invalid refresh token"})
		return
	}
	u, e := s.store.Find(r.Context(), id)
	if e != nil || !u.Active {
		write(w, 401, map[string]string{"error": "account unavailable"})
		return
	}
	out, e := s.tokens(r.Context(), u)
	if e != nil {
		write(w, 500, map[string]string{"error": "cannot create session"})
		return
	}
	write(w, 200, out)
}
func (s *Server) logout(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		write(w, 405, map[string]string{"error": "method not allowed"})
		return
	}
	var q struct {
		RefreshToken string `json:"refresh_token"`
	}
	if decode(r, &q) != nil {
		write(w, 400, map[string]string{"error": "invalid request"})
		return
	}
	_, _ = s.store.UseRefresh(r.Context(), q.RefreshToken)
	write(w, 204, nil)
}
func (s *Server) me(w http.ResponseWriter, r *http.Request) {
	if r.Method != "GET" {
		write(w, 405, map[string]string{"error": "method not allowed"})
		return
	}
	write(w, 200, userFrom(r))
}
func admin(w http.ResponseWriter, r *http.Request) bool {
	if userFrom(r).Role != "admin" {
		write(w, 403, map[string]string{"error": "admin role required"})
		return false
	}
	return true
}
func (s *Server) users(w http.ResponseWriter, r *http.Request) {
	if !admin(w, r) {
		return
	}
	switch r.Method {
	case "GET":
		v, e := s.store.List(r.Context())
		if e != nil {
			write(w, 500, map[string]string{"error": "cannot list users"})
			return
		}
		write(w, 200, v)
	case "POST":
		var q struct {
			Username, Password, Role string
			Active                   *bool `json:"active"`
		}
		if decode(r, &q) != nil || q.Username == "" || len(q.Password) < 12 || !validRole(q.Role) {
			write(w, 400, map[string]string{"error": "username, password (12+ chars), and valid role are required"})
			return
		}
		h, e := auth.HashPassword(q.Password)
		if e != nil {
			write(w, 500, map[string]string{"error": "cannot protect password"})
			return
		}
		active := true
		if q.Active != nil {
			active = *q.Active
		}
		v, e := s.store.Create(r.Context(), q.Username, h, q.Role, active)
		if e != nil {
			write(w, 409, map[string]string{"error": "username already exists"})
			return
		}
		s.store.Audit(r.Context(), userFrom(r).ID, "user.create", "user:"+v.Username, "")
		write(w, 201, v)
	default:
		write(w, 405, map[string]string{"error": "method not allowed"})
	}
}
func validRole(v string) bool { return v == "admin" || v == "operator" || v == "viewer" }
func (s *Server) user(w http.ResponseWriter, r *http.Request) {
	if !admin(w, r) {
		return
	}
	if r.Method != "PATCH" {
		write(w, 405, map[string]string{"error": "method not allowed"})
		return
	}
	id, e := strconv.ParseInt(strings.TrimPrefix(r.URL.Path, "/api/v1/users/"), 10, 64)
	if e != nil {
		write(w, 400, map[string]string{"error": "invalid user id"})
		return
	}
	var q struct {
		Role   string
		Active *bool `json:"active"`
	}
	if decode(r, &q) != nil || !validRole(q.Role) || q.Active == nil {
		write(w, 400, map[string]string{"error": "valid role and active are required"})
		return
	}
	v, e := s.store.Update(r.Context(), id, q.Role, *q.Active)
	if errors.Is(e, sql.ErrNoRows) {
		write(w, 404, map[string]string{"error": "user not found"})
		return
	}
	if e != nil {
		write(w, 500, map[string]string{"error": "cannot update user"})
		return
	}
	s.store.Audit(r.Context(), userFrom(r).ID, "user.update", "user:"+v.Username, "")
	write(w, 200, v)
}
