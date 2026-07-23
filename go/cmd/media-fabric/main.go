package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"

	"github.com/media-fabric/media-fabric/internal/media"
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

	stop := make(chan os.Signal, 1)
	signal.Notify(stop, syscall.SIGINT, syscall.SIGTERM)
	<-stop
}
