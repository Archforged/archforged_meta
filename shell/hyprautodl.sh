#!/usr/bin/env bash
set -euo pipefail

# Update system
echo "Updating system..."
sudo pacman -Syu --needed

# List of Hypr Ecosystem packages
HYPR_PACKAGES=(
  "hyprpaper"
  "hyprpicker"
  "xdg-desktop-portal-hyprland"
  "hypridle"
  "hyprlock"
  "hyprtoolkit"
  "hyprsysteminfo"
  "hyprlauncher"
  "hyprland-qtutils"
  "hyprsunset"
)

# Install via pacman or AUR
# Some might be in the official repos, others only in AUR.
echo "Installing Hyprland ecosystem apps..."
for pkg in "${HYPR_PACKAGES[@]}"; do
  if pacman -Ss --quiet "$pkg" | grep -q "^community/"; then
    echo "Installing $pkg from official repo"
    sudo pacman -S --needed "$pkg"
  else
    echo "Installing $pkg from AUR"
    yay -S --noconfirm "$pkg"
  fi
done

echo "Done installing Hyprland ecosystem apps."