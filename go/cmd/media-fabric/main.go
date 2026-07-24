package main

import (
	"context"
	"flag"
	"fmt"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/media-fabric/media-fabric/internal/api"
	"github.com/media-fabric/media-fabric/internal/auth"
	"github.com/media-fabric/media-fabric/internal/config"
	"github.com/media-fabric/media-fabric/internal/media"
	"github.com/media-fabric/media-fabric/internal/store"
)

func main() {
	configPath := flag.String("c", "conf/media-fabric.conf", "media-fabric config path")
	flag.StringVar(configPath, "config", "conf/media-fabric.conf", "media-fabric config path")
	disableManagementSocket := flag.Bool("disable-management-socket", false, "disable the mfcli Unix socket")
	command := flag.String("command", "", "run one compatibility management command then exit")
	flag.Parse()

	runtime, err := media.NewRuntime()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	defer runtime.Close()

	if err := runtime.Start(*configPath, !*disableManagementSocket); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	defer runtime.Stop()

	if *command != "" {
		response, err := runtime.Command(*command)
		if response != "" {
			fmt.Print(response)
		}
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		return
	}
	apiConfig, err := config.LoadAPI(*configPath)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	var httpServer *http.Server
	if apiConfig.Enabled {
		ctx := context.Background()
		userStore, err := store.Open(ctx, apiConfig.DatabaseURL)
		if err != nil {
			fmt.Fprintln(os.Stderr, "API database startup failed:", err)
			os.Exit(1)
		}
		defer userStore.Close()
		passwordHash, err := auth.HashPassword(apiConfig.BootstrapPassword)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		if err = userStore.Bootstrap(ctx, apiConfig.BootstrapUsername, passwordHash); err != nil {
			fmt.Fprintln(os.Stderr, "API bootstrap failed:", err)
			os.Exit(1)
		}
		httpServer = &http.Server{Addr: apiConfig.ListenAddress, Handler: api.New(apiConfig, userStore).Handler(), ReadHeaderTimeout: 5 * time.Second}
		go func() {
			if err := httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
				fmt.Fprintln(os.Stderr, "API server failed:", err)
			}
		}()
		fmt.Println("API listening on", apiConfig.ListenAddress)
	}

	stop := make(chan os.Signal, 1)
	signal.Notify(stop, syscall.SIGINT, syscall.SIGTERM)
	<-stop
	if httpServer != nil {
		_ = httpServer.Shutdown(context.Background())
	}
}
