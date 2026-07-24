package auth

import (
	"testing"
	"time"
)

func TestPasswordAndToken(t *testing.T) {
	h, e := HashPassword("correct horse battery staple")
	if e != nil || !VerifyPassword("correct horse battery staple", h) || VerifyPassword("wrong", h) {
		t.Fatal("password verification failed")
	}
	token, e := Sign("01234567890123456789012345678901", Claims{Subject: "1", Role: "admin", Kind: "access", ExpiresAt: time.Now().Add(time.Minute).Unix()})
	if e != nil {
		t.Fatal(e)
	}
	c, e := Verify("01234567890123456789012345678901", token, "access")
	if e != nil || c.Subject != "1" {
		t.Fatal("token verification failed")
	}
}
