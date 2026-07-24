package store

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"errors"
	"fmt"
	_ "github.com/lib/pq"
	"time"
)

type User struct {
	ID        int64     `json:"id"`
	Username  string    `json:"username"`
	Role      string    `json:"role"`
	Active    bool      `json:"active"`
	CreatedAt time.Time `json:"created_at"`
}
type Store struct{ db *sql.DB }

func Open(ctx context.Context, url string) (*Store, error) {
	db, e := sql.Open("postgres", url)
	if e != nil {
		return nil, e
	}
	if e = db.PingContext(ctx); e != nil {
		db.Close()
		return nil, e
	}
	s := &Store{db}
	_, e = db.ExecContext(ctx, `CREATE TABLE IF NOT EXISTS users(id BIGSERIAL PRIMARY KEY,username TEXT UNIQUE NOT NULL,password_hash TEXT NOT NULL,role TEXT NOT NULL,active BOOLEAN NOT NULL DEFAULT TRUE,created_at TIMESTAMPTZ NOT NULL DEFAULT now(),updated_at TIMESTAMPTZ NOT NULL DEFAULT now()); CREATE TABLE IF NOT EXISTS refresh_tokens(token_hash TEXT PRIMARY KEY,user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,expires_at TIMESTAMPTZ NOT NULL,revoked_at TIMESTAMPTZ); CREATE TABLE IF NOT EXISTS audit_logs(id BIGSERIAL PRIMARY KEY,actor_id BIGINT,action TEXT NOT NULL,target TEXT NOT NULL,detail TEXT NOT NULL DEFAULT '',created_at TIMESTAMPTZ NOT NULL DEFAULT now());`)
	if e != nil {
		db.Close()
		return nil, e
	}
	return s, nil
}
func (s *Store) Close() error { return s.db.Close() }
func (s *Store) Bootstrap(ctx context.Context, u, h string) error {
	var n int
	if e := s.db.QueryRowContext(ctx, "SELECT count(*) FROM users").Scan(&n); e != nil {
		return e
	}
	if n > 0 {
		return nil
	}
	_, e := s.db.ExecContext(ctx, "INSERT INTO users(username,password_hash,role) VALUES($1,$2,'admin')", u, h)
	return e
}
func (s *Store) FindByUsername(ctx context.Context, u string) (User, string, error) {
	var x User
	var h string
	e := s.db.QueryRowContext(ctx, "SELECT id,username,password_hash,role,active,created_at FROM users WHERE username=$1", u).Scan(&x.ID, &x.Username, &h, &x.Role, &x.Active, &x.CreatedAt)
	return x, h, e
}
func (s *Store) Find(ctx context.Context, id int64) (User, error) {
	var x User
	e := s.db.QueryRowContext(ctx, "SELECT id,username,role,active,created_at FROM users WHERE id=$1", id).Scan(&x.ID, &x.Username, &x.Role, &x.Active, &x.CreatedAt)
	return x, e
}
func (s *Store) List(ctx context.Context) ([]User, error) {
	rows, e := s.db.QueryContext(ctx, "SELECT id,username,role,active,created_at FROM users ORDER BY id")
	if e != nil {
		return nil, e
	}
	defer rows.Close()
	var r []User
	for rows.Next() {
		var x User
		if e = rows.Scan(&x.ID, &x.Username, &x.Role, &x.Active, &x.CreatedAt); e != nil {
			return nil, e
		}
		r = append(r, x)
	}
	return r, rows.Err()
}
func (s *Store) Create(ctx context.Context, u, h, role string, active bool) (User, error) {
	var x User
	e := s.db.QueryRowContext(ctx, "INSERT INTO users(username,password_hash,role,active) VALUES($1,$2,$3,$4) RETURNING id,username,role,active,created_at", u, h, role, active).Scan(&x.ID, &x.Username, &x.Role, &x.Active, &x.CreatedAt)
	return x, e
}
func (s *Store) Update(ctx context.Context, id int64, role string, active bool) (User, error) {
	var x User
	e := s.db.QueryRowContext(ctx, "UPDATE users SET role=$2,active=$3,updated_at=now() WHERE id=$1 RETURNING id,username,role,active,created_at", id, role, active).Scan(&x.ID, &x.Username, &x.Role, &x.Active, &x.CreatedAt)
	return x, e
}
func hash(t string) string { v := sha256.Sum256([]byte(t)); return fmt.Sprintf("%x", v[:]) }
func (s *Store) SaveRefresh(ctx context.Context, id int64, t string, exp time.Time) error {
	_, e := s.db.ExecContext(ctx, "INSERT INTO refresh_tokens(token_hash,user_id,expires_at) VALUES($1,$2,$3)", hash(t), id, exp)
	return e
}
func (s *Store) UseRefresh(ctx context.Context, t string) (int64, error) {
	var id int64
	e := s.db.QueryRowContext(ctx, "UPDATE refresh_tokens SET revoked_at=now() WHERE token_hash=$1 AND revoked_at IS NULL AND expires_at>now() RETURNING user_id", hash(t)).Scan(&id)
	if errors.Is(e, sql.ErrNoRows) {
		return 0, errors.New("refresh token is invalid")
	}
	return id, e
}
func (s *Store) Audit(ctx context.Context, id int64, act, target, detail string) {
	_, _ = s.db.ExecContext(ctx, "INSERT INTO audit_logs(actor_id,action,target,detail) VALUES($1,$2,$3,$4)", id, act, target, detail)
}
