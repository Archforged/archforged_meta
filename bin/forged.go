// forged.go
package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

const version = "2.1.0"

const (
	reset   = "\033[0m"
	bold    = "\033[1m"
	green   = "\033[32m"
	yellow  = "\033[33m"
	red     = "\033[31m"
	cyan    = "\033[36m"
	blue    = "\033[34m"
	magenta = "\033[35m"
)

func main() {
	if len(os.Args) < 2 {
		usage()
	}

	switch os.Args[1] {
	case "-B", "--bin":
		handleBin(os.Args[2:])
	case "-G", "--git":
		handleGit(os.Args[2:])
	case "-Syu", "--sync-upgrade":
		fullUpgrade()
	case "-v", "--version":
		fmt.Printf("%s forged %s — the Archforged package manager\n", green+"forged"+reset, cyan+version+reset)
	default:
		usage()
	}
}

func usage() {
	fmt.Printf(`
%s%s%s
%s

Usage:
  %s -B <package>           → install from repos or AUR
  %s -G <url>               → clone & auto-build git project
  %s -Syu                   → full system upgrade
  %s -v                     → show version

Examples:
  %s hyprland
  %s visual-studio-code-bin
  %s https://github.com/hyprwm/hyprpaper
`, magenta+bold+"forged"+reset, reset, cyan+version+reset, yellow+"The future of ricing"+reset,
		green+"forged"+reset, green+"forged"+reset, green+"forged"+reset, green+"forged"+reset,
		green+"forged -B"+reset, green+"forged -B"+reset, green+"forged -G"+reset)
	os.Exit(0)
}

func handleBin(args []string) {
	if len(args) == 0 {
		fmt.Printf("%s package name required\n", red+"error"+reset)
		os.Exit(1)
	}
	pkg := args[0]
	fmt.Printf("%s Installing %s...\n", blue+"install"+reset, yellow+pkg+reset)

	// Try official repos
	if run("pacman", "-Si", pkg) == nil {
		if err := run("sudo", "pacman", "-S", "--noconfirm", pkg); err != nil {
			fmt.Printf("%s failed to install %s from official repos\n", red+"error"+reset, yellow+pkg+reset)
			os.Exit(1)
		}
		fmt.Printf("%s %s installed from official repositories\n", green+"success"+reset, pkg)
		return
	}

	// AUR fallback
	var helperCmd string
	if hasYay() {
		helperCmd = "yay"
	} else if hasParu() {
		helperCmd = "paru"
	} else {
		fmt.Printf("%s Installing yay first...\n", yellow+"warning"+reset)
		installYay()
		helperCmd = "yay"
	}

	if err := run(helperCmd, "-S", "--noconfirm", pkg); err != nil {
		fmt.Printf("%s failed to install %s from AUR\n", red+"error"+reset, yellow+pkg+reset)
		os.Exit(1)
	}

	fmt.Printf("%s %s installed from AUR\n", green+"success"+reset, pkg)
}

func handleGit(args []string) {
	if len(args) == 0 {
		fmt.Printf("%s git URL required\n", red+"error"+reset)
		os.Exit(1)
	}
	url := args[0]
	name := strings.TrimSuffix(filepath.Base(url), ".git")
	dir := fmt.Sprintf("/tmp/forged-%s-%d", name, time.Now().Unix())

	fmt.Printf("%s Cloning %s → %s\n", cyan+"download"+reset, yellow+name+reset, dir)
	if err := run("git", "clone", "--depth=1", url, dir); err != nil {
		fmt.Printf("%s git clone failed for %s\n", red+"error"+reset, yellow+url+reset)
		os.Exit(1)
	}
	os.Chdir(dir)

	switch {
	case fileExists("PKGBUILD"):
		buildPkgbuild(name)
	case fileExists("meson.build"):
		buildMeson(name)
	case fileExists("CMakeLists.txt"):
		buildCMake(name)
	case fileExists("package.json"):
		buildNode(name)
	case fileExists("pom.xml"):
		buildMaven(name)
	case fileExists("build.gradle"), fileExists("gradlew"):
		buildGradle(name)
	case fileExists("configure"), fileExists("Makefile.am"):
		buildAutotools(name)
	case fileExists("go.mod"):
		buildGo(name)
	default:
		fmt.Printf("%s No build system detected — dropping into shell\n", yellow+"warning"+reset)
		run("bash")
	}
}

func exitOnErr(err error, name string, step string) {
	if err != nil {
		fmt.Printf("%s %s failed for %s\n", red+"error"+reset, step, yellow+name+reset)
		os.Exit(1)
	}
}

func buildPkgbuild(name string) {
	fmt.Printf("%s PKGBUILD → makepkg -si\n", cyan+"build"+reset)
	exitOnErr(run("makepkg", "-si", "--noconfirm"), name, "makepkg")
}
func buildMeson(name string) {
	fmt.Printf("%s Meson → ninja install\n", cyan+"build"+reset)
	exitOnErr(run("meson", "setup", "build", "--prefix=/usr"), name, "meson setup")
	exitOnErr(run("ninja", "-C", "build"), name, "ninja build")
	exitOnErr(run("sudo", "ninja", "-C", "build", "install"), name, "ninja install")
}
func buildCMake(name string) {
	fmt.Printf("%s CMake → make install\n", cyan+"build"+reset)
	exitOnErr(run("cmake", "-Bbuild", "-DCMAKE_INSTALL_PREFIX=/usr"), name, "cmake")
	exitOnErr(run("make", "-Cbuild", "-j$(nproc)"), name, "make")
	exitOnErr(run("sudo", "make", "-Cbuild", "install"), name, "make install")
}
func buildNode(name string) {
	fmt.Printf("%s Node.js → npm install && build\n", cyan+"build"+reset)
	exitOnErr(run("npm", "install"), name, "npm install")
	if fileExists("webpack.config.js") || fileExists("vite.config.js") {
		exitOnErr(run("npm", "run", "build"), name, "npm run build")
	}
}
func buildMaven(name string) {
	fmt.Printf("%s Maven → mvn install\n", cyan+"build"+reset)
	exitOnErr(run("mvn", "clean", "install"), name, "mvn install")
}
func buildGradle(name string) {
	fmt.Printf("%s Gradle → ./gradlew build\n", cyan+"build"+reset)
	if fileExists("gradlew") {
		exitOnErr(run("chmod", "+x", "gradlew"), name, "chmod gradlew")
		exitOnErr(run("./gradlew", "build"), name, "./gradlew build")
	} else {
		exitOnErr(run("gradle", "build"), name, "gradle build")
	}
}
func buildAutotools(name string) {
	fmt.Printf("%s Autotools → ./configure && make install\n", cyan+"build"+reset)
	if !fileExists("configure") {
		exitOnErr(run("autoreconf", "-fi"), name, "autoreconf")
	}
	exitOnErr(run("./configure", "--prefix=/usr"), name, "./configure")
	exitOnErr(run("make", "-j$(nproc)"), name, "make")
	exitOnErr(run("sudo", "make", "install"), name, "make install")
}
func buildGo(name string) {
	fmt.Printf("%s Go → go build && install\n", cyan+"build"+reset)
	exitOnErr(run("go", "build", "."), name, "go build")
	// **BUG FIX**: Install './{name}' (the binary you just built), not os.Args[0]
	exitOnErr(run("sudo", "install", "./"+name, "/usr/local/bin/"), name, "go install")
}

func fullUpgrade() {
	fmt.Printf("%s Upgrading system + AUR...\n", blue+"upgrade"+reset)
	exitOnErr(run("sudo", "pacman", "-Syu", "--noconfirm"), "system", "pacman upgrade")
	if hasYay() {
		exitOnErr(run("yay", "-Syu", "--noconfirm"), "AUR", "yay upgrade")
	} else if hasParu() {
		exitOnErr(run("paru", "-Syu", "--noconfirm"), "AUR", "paru upgrade")
	}
	fmt.Printf("%s System fully upgraded!\n", green+"success"+reset)
}

func run(name string, args ...string) error {
	cmd := exec.Command(name, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin
	return cmd.Run()
}

func fileExists(p string) bool { _, err := os.Stat(p); return err == nil }
func hasYay() bool            { return exec.Command("which", "yay").Run() == nil }
func hasParu() bool           { return exec.Command("which", "paru").Run() == nil }

func installYay() {
	if err := run("git", "clone", "https://aur.archlinux.org/yay.git", "/tmp/yay"); err != nil {
		fmt.Printf("%s failed to clone yay\n", red+"error"+reset)
		os.Exit(1)
	}
	os.Chdir("/tmp/yay")
	if err := run("makepkg", "-si", "--noconfirm"); err != nil {
		fmt.Printf("%s failed to build yay\n", red+"error"+reset)
		os.Exit(1)
	}
}
