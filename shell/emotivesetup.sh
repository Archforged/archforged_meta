#!/usr/bin/env bash
set -e
clear
echo "=================================================================="
echo "  EMOTIVE-ENGINE – FIRST-TIME SETUP"
echo "  Made with <3 and AI by Electron Micro Computing"
echo "  https://ko-fi.com/electronmicrocomputing"
echo "=================================================================="
echo
echo "This will:"
echo " • Install all dependencies (hyprshade, hyprpaper, grim, ffmpeg, etc.)"
echo " • Install Python packages"
echo " • Create the hyprshade wrapper script"
echo " • Download the window tint shader"
echo " • Add exec-once lines to your Hyprland config"
echo " • Restart audio + reload Hyprland"
echo
read -p "Press ENTER to begin (or Ctrl+C to cancel)..."

# 1. Install system packages
echo "Installing system packages..."
sudo pacman -Syu --needed --noconfirm \
    python python-pip python-numpy \
    pipewire pipewire-pulse wireplumber \
    aubio playerctl mpv socat grim ffmpeg \
    hyprpaper hyprshade swww waytrogen \
    python-pillow python-requests sounddevice

# 2. Python deps
echo "Installing Python packages..."
pip install --user --upgrade --no-input sounddevice numpy aubio pillow requests --prefer-binary

# 3. Create directories
mkdir -p ~/.config/hypr/shaders

# 4. Download the perfect reactive inactive shader
echo "Downloading the shaders from Catbox"
curl -sL 
https://files.catbox.moe/ilkyhf.frag\
    -o ~/.config/hypr/shaders/inactive.frag

# 5. Create hyprshade wrapper
echo "Creating hyprshade uniform injector..."
cat > ~/.config/hypr/shaders/emotive_uniforms.sh <<'EOF'
#!/usr/bin/env bash
[[ -f /tmp/emotive_engine_uniforms ]] && source /tmp/emotive_engine_uniforms
echo "uniform vec3 accentColor = vec3($R, $G, $B);"
echo "uniform float beat = $BEAT;"
EOF
chmod +x ~/.config/hypr/shaders/emotive_uniforms.sh

# 6. Add to hyprland.conf (only if not already present)
HYPRLAND_CONF="$HOME/.config/hypr/hyprland.conf"
echo "Adding lines to hyprland.conf..."

{
    grep -q "emotive-engine.py" "$HYPRLAND_CONF" || echo "exec-once = python3 ~/bin/emotive-engine.py" >> "$HYPRLAND_CONF"
    grep -q "hyprshade auto" "$HYPRLAND_CONF" || echo "exec-once = hyprshade auto" >> "$HYPRLAND_CONF"
    grep -q "screen_shader" "$HYPRLAND_CONF" || cat <<'ADD' >> "$HYPRLAND_CONF"

# === EMOTIVE-ENGINE FULL RICE ===
decoration {
    screen_shader = [[~/.config/hypr/shaders/inactive.frag]]
}
ADD
} || true

# 7. Create bin dir + symlink script
echo "Downloading the script from Catbox, and placing it in /bin…"
mkdir -p ~/bin
curl -sL 
https://files.catbox.moe/de0c4k.py
    -o ~/bin/emotive-engine.py
chmod +x ~/bin/emotive-engine.py

# 8. Final restart
echo "Restarting PipeWire..."
systemctl --user try-restart pipewire pipewire-pulse wireplumber

echo "Reloading Hyprland (this will refresh your session)..."
hyprctl reload || true

echo
echo "=================================================================="

echo “All done!”
echo "Just log out and back in (or run 'hyprctl reload') if needed."
echo "Post your setup on r/unixporn and think of me, mkay?"
echo
echo "Made with <3 and AI at Electron Micro Computing"
echo "https://ko-fi.com/electronmicrocomputing"
echo "=================================================================="
