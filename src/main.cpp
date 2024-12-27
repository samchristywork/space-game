#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

static const char *text_vert_src = R"(
#version 330 core
layout(location = 0) in vec4 vtx; // xy = screen pos, zw = uv
uniform mat4 u_proj;
out vec2 uv;
void main() {
    gl_Position = u_proj * vec4(vtx.xy, 0.0, 1.0);
    uv = vtx.zw;
}
)";

static const char *text_frag_src = R"(
#version 330 core
in vec2 uv;
uniform sampler2D u_font;
uniform vec3 u_text_color;
out vec4 out_color;
void main() {
    out_color = vec4(u_text_color, texture(u_font, uv).r);
}
)";

struct Glyph {
  float tx, ty, tw, th;
  int bx, by, bw, bh, advance;
};
static Glyph g_glyphs[128];
static GLuint g_font_tex = 0;
static GLuint g_text_prog = 0;
static GLuint g_text_vao = 0;
static GLuint g_text_vbo = 0;
static int g_font_size = 0;

static bool text_init(int pixel_size) {
  const char *font_paths[] = {
      "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
      "/usr/share/fonts/noto/NotoSans-Regular.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
      nullptr,
  };

  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    fprintf(stderr, "FreeType init failed\n");
    return false;
  }

  FT_Face face = nullptr;
  for (int i = 0; font_paths[i]; i++)
    if (FT_New_Face(ft, font_paths[i], 0, &face) == 0)
      break;
  if (!face) {
    fprintf(stderr, "No usable font found\n");
    FT_Done_FreeType(ft);
    return false;
  }

  FT_Set_Pixel_Sizes(face, 0, pixel_size);
  g_font_size = pixel_size;

  // Build a single-row atlas (512 wide × 2*pixel_size tall)
  const int AW = 2048, AH = pixel_size * 2;
  unsigned char *atlas = new unsigned char[AW * AH]();
  int cx = 0;
  for (int c = 32; c < 128; c++) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER))
      continue;
    FT_GlyphSlot gs = face->glyph;
    int bw = gs->bitmap.width, bh = (int)gs->bitmap.rows;
    if (cx + bw >= AW)
      break;
    for (int row = 0; row < bh; row++)
      memcpy(&atlas[row * AW + cx], &gs->bitmap.buffer[row * gs->bitmap.pitch],
             bw);
    g_glyphs[c] = {(float)cx / AW,
                   0.0f,
                   (float)bw / AW,
                   (float)bh / AH,
                   gs->bitmap_left,
                   gs->bitmap_top,
                   bw,
                   bh,
                   (int)(gs->advance.x >> 6)};
    cx += bw + 1;
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glGenTextures(1, &g_font_tex);
  glBindTexture(GL_TEXTURE_2D, g_font_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, AW, AH, 0, GL_RED, GL_UNSIGNED_BYTE,
               atlas);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  delete[] atlas;

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  // VAO/VBO (dynamic; 6 verts × 4 floats per glyph)
  glGenVertexArrays(1, &g_text_vao);
  glGenBuffers(1, &g_text_vbo);
  glBindVertexArray(g_text_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_text_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * 256, nullptr,
               GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);

  // Build the text shader program using the existing helper
  // (declared later; we only call text_draw after main sets up OpenGL)
  return true;
}

// Draw text at screen position (x, y) with y=0 at top-left.
// Must be called after text_init() and after g_text_prog is linked.
static float text_width(const char *str) {
  float w = 0;
  for (const char *p = str; *p; p++) {
    int c = (unsigned char)*p;
    if (c >= 32 && c < 128)
      w += g_glyphs[c].advance;
    else
      w += g_font_size * 0.5f;
  }
  return w;
}

static void text_draw(const char *str, float x, float y, glm::vec3 color,
                      int sw, int sh) {
  if (!g_text_prog || !g_font_tex)
    return;

  glUseProgram(g_text_prog);
  glm::mat4 proj = glm::ortho(0.0f, (float)sw, (float)sh, 0.0f);
  glUniformMatrix4fv(glGetUniformLocation(g_text_prog, "u_proj"), 1, GL_FALSE,
                     glm::value_ptr(proj));
  glUniform3f(glGetUniformLocation(g_text_prog, "u_text_color"), color.r,
              color.g, color.b);
  glUniform1i(glGetUniformLocation(g_text_prog, "u_font"), 0);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_font_tex);
  glBindVertexArray(g_text_vao);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  std::vector<float> verts;
  verts.reserve(strlen(str) * 24);
  float cx = x;
  for (const char *p = str; *p; p++) {
    int c = (unsigned char)*p;
    if (c < 32 || c >= 128) {
      cx += g_font_size * 0.5f;
      continue;
    }
    const Glyph &g = g_glyphs[c];
    if (g.bw == 0) {
      cx += g.advance;
      continue;
    }
    float x0 = cx + g.bx, y0 = y + (g_font_size - g.by);
    float x1 = x0 + g.bw, y1 = y0 + g.bh;
    float u0 = g.tx, v0 = g.ty, u1 = g.tx + g.tw, v1 = g.ty + g.th;
    float q[] = {x0, y0, u0, v0, x0, y1, u0, v1, x1, y1, u1, v1,
                 x0, y0, u0, v0, x1, y1, u1, v1, x1, y0, u1, v0};
    verts.insert(verts.end(), q, q + 24);
    cx += g.advance;
  }

  glBindBuffer(GL_ARRAY_BUFFER, g_text_vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float),
                  verts.data());
  glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 4));
  glDisable(GL_BLEND);
}

static const char *vert_src = R"(
#version 330 core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 v_color;
uniform mat4 mvp;
uniform float u_point_size;
out vec3 frag_color;
void main() {
    gl_Position = mvp * vec4(pos, 1.0);
    gl_PointSize = u_point_size;
    frag_color = v_color;
}
)";

static const char *frag_src = R"(
#version 330 core
in vec3 frag_color;
uniform vec3  u_color;
uniform bool  u_use_vertex_color;
uniform bool  u_is_star;
uniform float u_glow_falloff;
out vec4 color;
void main() {
    vec3 c = u_use_vertex_color ? frag_color : u_color;
    if (u_is_star) {
        float d = length(gl_PointCoord - 0.5) * 2.0; // 0=centre, 1=edge
        float a = exp(-d * u_glow_falloff);
        color = vec4(c * a, a);
    } else {
        color = vec4(c, 1.0);
    }
}
)";

static GLuint compile_shader(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);

  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    fprintf(stderr, "Shader compile error: %s\n", log);
    exit(1);
  }
  return s;
}

static GLuint build_program(const char *vs_src, const char *fs_src) {
  GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);

  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);

  GLint ok;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
    fprintf(stderr, "Program link error: %s\n", log);
    exit(1);
  }

  glDeleteShader(vs);
  glDeleteShader(fs);
  return prog;
}

struct Planet {
  glm::vec3 star_pos;
  float orbit_radius, orbit_tilt, orbit_period, orbit_angle;
  GLuint vao, vbo, ebo;
  GLsizei idx_count;
  GLuint orbit_vao, orbit_vbo;
};
static std::vector<Planet> g_planets;

// UV sphere: pos(3)+color(3) interleaved, indexed triangles
static void gen_sphere(std::vector<float> &verts, std::vector<GLuint> &idx,
                       float r, int lat, int lon, glm::vec3 col) {
  for (int i = 0; i <= lat; i++) {
    float theta = (float)M_PI * i / lat;
    for (int j = 0; j <= lon; j++) {
      float phi = 2.0f * (float)M_PI * j / lon;
      verts.insert(verts.end(),
                   {r * sinf(theta) * cosf(phi), r * cosf(theta),
                    r * sinf(theta) * sinf(phi), col.r, col.g, col.b});
    }
  }
  for (int i = 0; i < lat; i++)
    for (int j = 0; j < lon; j++) {
      GLuint a = i * (lon + 1) + j, b = a + lon + 1;
      idx.insert(idx.end(), {a, b, a + 1, b, b + 1, a + 1});
    }
}

// Local stars scattered around the scene.
// Colors approximate stellar spectral types (O=blue → M=red).
struct LocalStar {
  glm::vec3 pos;
  glm::vec3 color; // base tint; glow layers are derived from this
};

static const LocalStar LOCAL_STARS[] = {
    // Our sun (G-type, yellow) at origin
    {{0.0f, 0.0f, 0.0f}, {1.0f, 0.90f, 0.60f}},
    // Neighbours – varied spectral types and distances
    {{4.0f, 1.5f, -3.0f}, {0.6f, 0.75f, 1.00f}},   // B – blue-white
    {{-5.0f, 0.5f, 2.0f}, {1.0f, 0.40f, 0.15f}},   // M – red dwarf
    {{3.0f, -2.0f, 5.0f}, {1.0f, 0.95f, 0.85f}},   // A – white
    {{-2.5f, 3.0f, -6.0f}, {1.0f, 0.70f, 0.35f}},  // K – orange
    {{7.0f, 0.0f, 1.0f}, {0.55f, 0.65f, 1.00f}},   // O – hot blue
    {{-6.0f, -1.0f, -4.0f}, {1.0f, 0.95f, 0.75f}}, // F – yellow-white
    {{1.0f, 5.0f, 4.0f}, {1.0f, 0.50f, 0.20f}},    // K – orange giant
    {{-3.0f, -3.5f, 6.0f}, {0.70f, 0.80f, 1.00f}}, // B – blue
    {{5.5f, 2.0f, -5.0f}, {1.0f, 0.35f, 0.10f}},   // M – red
    {{-7.0f, 1.5f, 3.0f}, {1.0f, 1.00f, 0.90f}},   // A – white
};

// Orbit camera state
struct Camera {
  float yaw = 0.0f;   // radians, rotation around Y axis
  float pitch = 0.3f; // radians, elevation
  float dist = 3.0f;  // distance from target
  glm::vec3 target{0.0f, 0.0f, 0.0f};

  glm::mat4 view() const {
    float x = dist * cosf(pitch) * sinf(yaw);
    float y = dist * sinf(pitch);
    float z = dist * cosf(pitch) * cosf(yaw);
    glm::vec3 eye = target + glm::vec3(x, y, z);
    return glm::lookAt(eye, target, glm::vec3(0, 1, 0));
  }
};

static Camera cam;
static float g_timescale = 1.0f;
static float target_yaw = cam.yaw;
static float target_pitch = cam.pitch;
static bool mmb_down = false;
static double last_mx = 0.0;
static double last_my = 0.0;

static void key_cb(GLFWwindow *, int key, int, int action, int mods) {
  if (action != GLFW_PRESS)
    return;
  bool shift = (mods & GLFW_MOD_SHIFT) != 0;
  switch (key) {
  case GLFW_KEY_X:
    target_yaw = glm::radians(shift ? -90.0f : 90.0f);
    target_pitch = 0.0f;
    break;
  case GLFW_KEY_Y:
    target_yaw = cam.yaw;
    target_pitch = glm::radians(shift ? -89.0f : 89.0f);
    break;
  case GLFW_KEY_Z:
    target_yaw = glm::radians(shift ? 180.0f : 0.0f);
    target_pitch = 0.0f;
    break;
  case GLFW_KEY_COMMA:
    g_timescale = fmaxf(g_timescale / 10.0f, 1e-4f);
    break;
  case GLFW_KEY_PERIOD:
    g_timescale = fminf(g_timescale * 10.0f, 1e6f);
    break;
  }
}

static void scroll_cb(GLFWwindow *, double, double dy) {
  cam.dist *= powf(0.9f, (float)dy);
  if (cam.dist < 0.01f)
    cam.dist = 0.01f;
}

static void mouse_button_cb(GLFWwindow *, int button, int action, int) {
  if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    mmb_down = (action == GLFW_PRESS);
}

static void cursor_pos_cb(GLFWwindow *win, double mx, double my) {
  double dx = mx - last_mx;
  double dy = my - last_my;
  last_mx = mx;
  last_my = my;

  if (!mmb_down)
    return;

  bool shift = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
               glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

  if (shift) {
    // Pan: move target in camera-local right/up plane
    float x = cosf(cam.pitch) * sinf(cam.yaw);
    float y = sinf(cam.pitch);
    float z = cosf(cam.pitch) * cosf(cam.yaw);
    glm::vec3 fwd = glm::normalize(glm::vec3(x, y, z));
    glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::cross(right, fwd);

    float pan_speed = cam.dist * 0.001f;
    cam.target += right * (float)dx * pan_speed;
    cam.target += up * (float)dy * pan_speed;
  } else {
    // Orbit
    const float sensitivity = 0.005f;
    cam.yaw -= (float)dx * sensitivity;
    cam.pitch += (float)dy * sensitivity;

    const float limit = glm::radians(89.0f);
    cam.pitch = glm::clamp(cam.pitch, -limit, limit);

    // Keep targets in sync so dragging doesn't fight a pending animation
    target_yaw = cam.yaw;
    target_pitch = cam.pitch;
  }
}

int main() {
  if (!glfwInit()) {
    fprintf(stderr, "Failed to init GLFW\n");
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *win = glfwCreateWindow(800, 600, "Space Game", nullptr, nullptr);
  if (!win) {
    fprintf(stderr, "Failed to create window\n");
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
  glfwSetKeyCallback(win, key_cb);
  glfwSetMouseButtonCallback(win, mouse_button_cb);
  glfwSetCursorPosCallback(win, cursor_pos_cb);
  glfwSetScrollCallback(win, scroll_cb);

  // Seed last mouse position to avoid a jump on first move
  glfwGetCursorPos(win, &last_mx, &last_my);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to init GLEW\n");
    return 1;
  }

  text_init(18);
  g_text_prog = build_program(text_vert_src, text_frag_src);

  GLuint prog = build_program(vert_src, frag_src);
  GLint mvp_loc = glGetUniformLocation(prog, "mvp");
  GLint color_loc = glGetUniformLocation(prog, "u_color");
  GLint use_vc_loc = glGetUniformLocation(prog, "u_use_vertex_color");
  GLint point_size_loc = glGetUniformLocation(prog, "u_point_size");
  GLint is_star_loc = glGetUniformLocation(prog, "u_is_star");
  GLint falloff_loc = glGetUniformLocation(prog, "u_glow_falloff");

  // Star: a single point at the origin (pos + color, no color used directly)
  float sun_vert[] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
  GLuint sun_vao, sun_vbo;
  glGenVertexArrays(1, &sun_vao);
  glGenBuffers(1, &sun_vbo);
  glBindVertexArray(sun_vao);
  glBindBuffer(GL_ARRAY_BUFFER, sun_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(sun_vert), sun_vert, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Generate planets for all stars
  // Orbital periods follow Kepler's 3rd law: T = base_period * r^1.5
  static constexpr int ORBIT_SEGS = 64;
  static constexpr float BASE_PERIOD = 20.0f; // seconds at r=1
  srand(1337);
  for (const auto &star : LOCAL_STARS) {
    int n = 1 + rand() % 10;
    float r = 0.15f + (rand() / (float)RAND_MAX) * 0.15f; // innermost radius
    for (int p = 0; p < n; p++) {
      Planet pl;
      pl.star_pos = star.pos;
      pl.orbit_radius = r;
      pl.orbit_tilt = (rand() / (float)RAND_MAX) * glm::radians(30.0f);
      pl.orbit_period = BASE_PERIOD * powf(r, 1.5f);
      pl.orbit_angle = (rand() / (float)RAND_MAX) * 2.0f * (float)M_PI;

      // Size and color by zone: rocky (inner) → gas giant → ice giant
      float size;
      glm::vec3 col;
      if (r < 0.5f) {
        size = 0.02f + (rand() / (float)RAND_MAX) * 0.03f;
        float t = rand() / (float)RAND_MAX;
        col = glm::mix(glm::vec3(0.55f, 0.38f, 0.28f),
                       glm::vec3(0.70f, 0.55f, 0.42f), t);
      } else if (r < 1.1f) {
        size = 0.05f + (rand() / (float)RAND_MAX) * 0.05f;
        float t = rand() / (float)RAND_MAX;
        col = glm::mix(glm::vec3(0.78f, 0.62f, 0.38f),
                       glm::vec3(0.68f, 0.50f, 0.32f), t);
      } else {
        size = 0.03f + (rand() / (float)RAND_MAX) * 0.04f;
        float t = rand() / (float)RAND_MAX;
        col = glm::mix(glm::vec3(0.38f, 0.58f, 0.80f),
                       glm::vec3(0.52f, 0.72f, 0.90f), t);
      }

      // Sphere mesh
      std::vector<float> sv;
      std::vector<GLuint> si;
      gen_sphere(sv, si, size, 16, 16, col);
      pl.idx_count = (GLsizei)si.size();
      glGenVertexArrays(1, &pl.vao);
      glGenBuffers(1, &pl.vbo);
      glGenBuffers(1, &pl.ebo);
      glBindVertexArray(pl.vao);
      glBindBuffer(GL_ARRAY_BUFFER, pl.vbo);
      glBufferData(GL_ARRAY_BUFFER, sv.size() * sizeof(float), sv.data(),
                   GL_STATIC_DRAW);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pl.ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, si.size() * sizeof(GLuint),
                   si.data(), GL_STATIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                            (void *)0);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                            (void *)(3 * sizeof(float)));
      glEnableVertexAttribArray(1);

      // Orbit line
      std::vector<float> ov;
      ov.reserve(ORBIT_SEGS * 6);
      for (int i = 0; i < ORBIT_SEGS; i++) {
        float a = 2.0f * (float)M_PI * i / ORBIT_SEGS;
        ov.insert(ov.end(), {r * cosf(a), r * sinf(pl.orbit_tilt) * sinf(a),
                             r * sinf(a), 0.35f, 0.35f, 0.45f});
      }
      glGenVertexArrays(1, &pl.orbit_vao);
      glGenBuffers(1, &pl.orbit_vbo);
      glBindVertexArray(pl.orbit_vao);
      glBindBuffer(GL_ARRAY_BUFFER, pl.orbit_vbo);
      glBufferData(GL_ARRAY_BUFFER, ov.size() * sizeof(float), ov.data(),
                   GL_STATIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                            (void *)0);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                            (void *)(3 * sizeof(float)));
      glEnableVertexAttribArray(1);

      g_planets.push_back(pl);
      r +=
          0.15f + (rand() / (float)RAND_MAX) * 0.35f; // next planet farther out
    }
  }

  // Orbit target indicator: 3 axis lines, each from -0.1 to +0.1
  // Interleaved: pos(3) + color(3)
  float axis_verts[] = {
      -0.1f, 0.0f,  0.0f,  1.0f, 0.2f, 0.2f, // X- red
      0.1f,  0.0f,  0.0f,  1.0f, 0.2f, 0.2f, // X+ red
      0.0f,  -0.1f, 0.0f,  0.2f, 1.0f, 0.2f, // Y- green
      0.0f,  0.1f,  0.0f,  0.2f, 1.0f, 0.2f, // Y+ green
      0.0f,  0.0f,  -0.1f, 0.2f, 0.4f, 1.0f, // Z- blue
      0.0f,  0.0f,  0.1f,  0.2f, 0.4f, 1.0f, // Z+ blue
  };

  GLuint axis_vao, axis_vbo;
  glGenVertexArrays(1, &axis_vao);
  glGenBuffers(1, &axis_vbo);

  glBindVertexArray(axis_vao);
  glBindBuffer(GL_ARRAY_BUFFER, axis_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(axis_verts), axis_verts, GL_STATIC_DRAW);
  // pos
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  // color (location 1)
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Stars: random points on a unit sphere, rendered with rotation-only view
  // so they appear infinitely distant regardless of camera position.
  static constexpr int NUM_STARS = 3000;
  std::vector<float> star_verts;
  star_verts.reserve(NUM_STARS * 6);
  srand(42);
  for (int i = 0; i < NUM_STARS; i++) {
    // Uniform distribution on sphere via normal-vector method
    float x = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
    float y = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
    float z = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
    float len = sqrtf(x * x + y * y + z * z);
    if (len < 1e-6f) {
      i--;
      continue;
    }
    x /= len;
    y /= len;
    z /= len;

    // Vary brightness: most stars dim, a few bright
    float b = 0.3f + 0.7f * powf(rand() / (float)RAND_MAX, 3.0f);
    star_verts.insert(star_verts.end(), {x, y, z, b, b, b});
  }

  GLuint star_vao, star_vbo;
  glGenVertexArrays(1, &star_vao);
  glGenBuffers(1, &star_vbo);
  glBindVertexArray(star_vao);
  glBindBuffer(GL_ARRAY_BUFFER, star_vbo);
  glBufferData(GL_ARRAY_BUFFER, star_verts.size() * sizeof(float),
               star_verts.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);

  double prev_time = glfwGetTime();

  while (!glfwWindowShouldClose(win)) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(win, GLFW_TRUE);

    double now = glfwGetTime();
    float dt = (float)(now - prev_time);
    prev_time = now;

    // Smoothly interpolate camera toward target orientation.
    // Take the shortest arc for yaw by normalising the difference to [-π, π].
    const float speed = 8.0f;
    float dyaw = target_yaw - cam.yaw;
    dyaw -= glm::round(dyaw / glm::two_pi<float>()) * glm::two_pi<float>();
    float t = 1.0f - expf(-speed * dt);
    cam.yaw += dyaw * t;
    cam.pitch += (target_pitch - cam.pitch) * t;

    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    glViewport(0, 0, w, h);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 proj =
        glm::perspective(glm::radians(45.0f), (float)w / h, 0.1f, 1000.0f);
    glm::mat4 view = cam.view();
    glm::mat4 mvp = proj * view;

    // Rotation-only view for stars (strip translation so they're at infinity)
    glm::mat4 view_rot = glm::mat4(glm::mat3(view));

    glUseProgram(prog);
    glUniform1f(point_size_loc, 1.0f);

    // Draw stars (depth writes off so they're always behind everything)
    glDepthMask(GL_FALSE);
    glUniform1i(use_vc_loc, 1);
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(proj * view_rot));
    glBindVertexArray(star_vao);
    glDrawArrays(GL_POINTS, 0, NUM_STARS);
    glDepthMask(GL_TRUE);

    // Draw local stars: three additive glow layers per star (outer → inner)
    glUniform1i(use_vc_loc, 0);
    glUniform1i(is_star_loc, 1);
    glBindVertexArray(sun_vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    for (const auto &s : LOCAL_STARS) {
      glm::mat4 star_mvp = mvp * glm::translate(glm::mat4(1.0f), s.pos);
      glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(star_mvp));

      glm::vec3 c = s.color;
      // Outer halo – tinted, soft
      glUniform1f(point_size_loc, 180.0f);
      glUniform1f(falloff_loc, 3.0f);
      glUniform3f(color_loc, c.r * 0.8f, c.g * 0.4f, c.b * 0.1f);
      glDrawArrays(GL_POINTS, 0, 1);
      // Mid glow
      glUniform1f(point_size_loc, 80.0f);
      glUniform1f(falloff_loc, 5.0f);
      glUniform3f(color_loc, c.r, c.g * 0.9f, c.b * 0.5f);
      glDrawArrays(GL_POINTS, 0, 1);
      // Core – white-hot
      glUniform1f(point_size_loc, 20.0f);
      glUniform1f(falloff_loc, 8.0f);
      glUniform3f(color_loc, c.r, c.g, c.b * 0.95f);
      glDrawArrays(GL_POINTS, 0, 1);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUniform1i(is_star_loc, 0);

    // Draw all planets and orbit lines
    glUniform1i(use_vc_loc, 1);
    glUniform1i(is_star_loc, 0);
    for (auto &pl : g_planets) {
      pl.orbit_angle +=
          dt * g_timescale * (2.0f * (float)M_PI / pl.orbit_period);
      pl.orbit_angle = fmodf(pl.orbit_angle, 2.0f * (float)M_PI);
      glm::vec3 pos =
          pl.star_pos + glm::vec3(pl.orbit_radius * cosf(pl.orbit_angle),
                                  pl.orbit_radius * sinf(pl.orbit_tilt) *
                                      sinf(pl.orbit_angle),
                                  pl.orbit_radius * sinf(pl.orbit_angle));

      glm::mat4 planet_mvp = mvp * glm::translate(glm::mat4(1.0f), pos);
      glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(planet_mvp));
      glBindVertexArray(pl.vao);
      glDrawElements(GL_TRIANGLES, pl.idx_count, GL_UNSIGNED_INT, nullptr);

      glm::mat4 orbit_mvp = mvp * glm::translate(glm::mat4(1.0f), pl.star_pos);
      glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(orbit_mvp));
      glBindVertexArray(pl.orbit_vao);
      glDrawArrays(GL_LINE_LOOP, 0, ORBIT_SEGS);
    }

    // Draw orbit target indicator
    float cursor_size = powf(10.0f, floorf(log10f(cam.dist)));
    glm::mat4 axis_mvp = mvp * glm::translate(glm::mat4(1.0f), cam.target) *
                         glm::scale(glm::mat4(1.0f), glm::vec3(cursor_size));
    glUniform1i(use_vc_loc, 1);
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(axis_mvp));
    glBindVertexArray(axis_vao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 6);

    // FPS counter: update display value once per second
    static int fps_frames = 0;
    static float fps_elapsed = 0.0f;
    static int fps_display = 0;
    fps_frames++;
    fps_elapsed += dt;
    if (fps_elapsed >= 1.0f) {
      fps_display = (int)(fps_frames / fps_elapsed + 0.5f);
      fps_frames = 0;
      fps_elapsed = 0.0f;
    }
    char fps_buf[16];
    snprintf(fps_buf, sizeof(fps_buf), "%d", fps_display);
    text_draw(fps_buf, 10.0f, 10.0f, {1.0f, 1.0f, 1.0f}, w, h);

    char pos_buf[64];
    snprintf(pos_buf, sizeof(pos_buf), "%.2f  %.2f  %.2f", cam.target.x,
             cam.target.y, cam.target.z);
    text_draw(pos_buf, 10.0f, 10.0f + g_font_size + 4.0f, {0.7f, 0.7f, 0.7f}, w,
              h);

    char ts_buf[32];
    snprintf(ts_buf, sizeof(ts_buf), "%gx", g_timescale);
    text_draw(ts_buf, w - text_width(ts_buf) - 10.0f, 10.0f, {0.8f, 0.8f, 0.8f},
              w, h);

    glfwSwapBuffers(win);
    glfwPollEvents();
  }

  for (auto &pl : g_planets) {
    glDeleteVertexArrays(1, &pl.vao);
    glDeleteBuffers(1, &pl.vbo);
    glDeleteBuffers(1, &pl.ebo);
    glDeleteVertexArrays(1, &pl.orbit_vao);
    glDeleteBuffers(1, &pl.orbit_vbo);
  }
  glDeleteVertexArrays(1, &sun_vao);
  glDeleteBuffers(1, &sun_vbo);
  glDeleteVertexArrays(1, &axis_vao);
  glDeleteBuffers(1, &axis_vbo);
  glDeleteVertexArrays(1, &star_vao);
  glDeleteBuffers(1, &star_vbo);
  glDeleteProgram(prog);
  glfwTerminate();
  return 0;
}
