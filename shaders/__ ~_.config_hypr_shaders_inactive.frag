// ~/.config/hypr/shaders/inactive.frag
// The one that makes people say "how the hell did you do that"

precision highp float;

uniform sampler2D sampler0;
uniform vec2 u_resolution;
uniform vec3 accentColor;   // from Emotive Engine (0.0–1.0)
uniform float beat;         // 1.0 on beat, 0.0 otherwise
uniform float time;

vec4 shaderMain() {
    vec2 uv = gl_FragCoord.xy / u_resolution;

    // Medium Gaussian-style blur (4-tap)
    float blurSize = 0.0023;
    vec4 blur = (
        texture(sampler0, uv + vec2( blurSize,  blurSize)) +
        texture(sampler0, uv + vec2(-blurSize,  blurSize)) +
        texture(sampler0, uv + vec2( blurSize, -blurSize)) +
        texture(sampler0, uv + vec2(-blurSize, -blurSize))
    ) * 0.25;

    // Chromatic aberration — pulses hard on beat
    float caStrength = 1.0 + beat * 0.15;
    vec2 ca_offset = (uv - 0.5) * 0.004 * caStrength;

    float r = texture(sampler0, uv + ca_offset).r;
    float g = texture(sampler0, uv).g;
    float b = texture(sampler0, uv - ca_offset).b;
    vec3 ca_color = vec3(r, g, b);

    // Blend blur + CA
    vec3 base = mix(ca_color, blur.rgb, 0.35);

    // Reactive tint — stronger on beat
    float tint_strength = 0.22 + beat * 0.25;
    vec3 tinted = mix(base, accentColor, tint_strength);

    // Subtle breathing vignette (always active)
    float vignette = smoothstep(0.8, 0.3, length(uv - 0.5));
    tinted *= 0.94 + 0.06 * vignette;

    return vec4(tinted, 1.0);
}