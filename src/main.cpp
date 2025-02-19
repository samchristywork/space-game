#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <ctime>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

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

  return true;
}

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
uniform float u_alpha;
out vec4 color;
void main() {
    vec3 c = u_use_vertex_color ? frag_color : u_color;
    if (u_is_star) {
        float d = length(gl_PointCoord - 0.5) * 2.0;
        float a = exp(-d * u_glow_falloff);
        color = vec4(c * a, a);
    } else {
        color = vec4(c, u_alpha);
    }
}
)";

static const char *img_vert_src = R"(
#version 330 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;
uniform mat4 u_proj;
out vec2 v_uv;
void main() {
    gl_Position = u_proj * vec4(pos, 0.0, 1.0);
    v_uv = uv;
}
)";

static const char *img_frag_src = R"(
#version 330 core
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 out_color;
void main() {
    out_color = texture(u_tex, v_uv);
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

enum ShipShape { SHIP_TRIANGLE, SHIP_SQUARE };

struct Spaceship {
  glm::vec3 pos;
  ShipShape shape;
  bool selected = false;
  bool has_move_target = false;
  glm::vec3 move_target{0.0f, 0.0f, 0.0f};
  std::vector<glm::vec3> waypoints; // wormhole stops for current leg
  std::vector<glm::vec3>
      pending_targets;  // user-queued destinations after move_target
  int formation_id = 0; // shared among ships in the same move order (0 = none)
  GLuint vao, vbo;
};
static std::vector<Spaceship> g_ships;

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

struct LocalStar {
  glm::vec3 pos;
  glm::vec3 color; // base tint; glow layers are derived from this
};

static const LocalStar LOCAL_STARS[] = {
    {{0.0f, 0.0f, 0.0f}, {1.0f, 0.90f, 0.60f}},    // G - yellow
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

struct WormholePair {
  glm::vec3 a, b;
};

static const WormholePair WORMHOLE_PAIRS[] = {
    {{2.0f, 1.0f, 3.0f}, {-4.0f, -1.5f, -2.0f}},
    {{-3.0f, 2.0f, 1.0f}, {5.0f, -2.0f, 4.0f}},
    {{1.0f, -2.5f, -5.0f}, {-1.5f, 3.5f, 2.0f}},
};
static constexpr int NUM_WORMHOLE_PAIRS =
    (int)(sizeof(WORMHOLE_PAIRS) / sizeof(WORMHOLE_PAIRS[0]));

struct Camera {
  float yaw = 0.0f;
  float pitch = 0.3f;
  float dist = 3.0f;
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

enum FormationType {
  FORMATION_HEX,
  FORMATION_LINE,
  FORMATION_WEDGE,
  FORMATION_WALL,
  FORMATION_BOX,
  FORMATION_SPHERE,
  FORMATION_RANDOM,
  FORMATION_COUNT
};
static FormationType g_formation = FORMATION_SPHERE;
static const char *FORMATION_NAMES[] = {"Hex", "Line",   "Wedge", "Wall",
                                        "Box", "Sphere", "Random"};

static float g_timescale = 1.0f;
static bool g_pick_pending = false;
static double g_pick_x = 0.0, g_pick_y = 0.0;
static float target_yaw = cam.yaw;
static float target_pitch = cam.pitch;
static bool mmb_down = false;
static bool g_rmb_down = false;
static std::vector<int> g_groups[10];
static int g_last_group_key = -1;
static double g_last_group_time = -1.0;
static double last_mx = 0.0;
static double last_my = 0.0;
static bool g_follow_mode = false;
static bool g_screenshot_pending = false;
static bool g_lmb_down = false;
static bool g_drag_active = false;
static bool g_drag_select_pending = false;
static double g_drag_start_x = 0.0, g_drag_start_y = 0.0;
static double g_drag_cur_x = 0.0, g_drag_cur_y = 0.0;

static bool g_uncapped = false;
static bool g_paused = false;

enum HoverType {
  HOVER_NONE,
  HOVER_STAR,
  HOVER_PLANET,
  HOVER_SHIP,
  HOVER_WORMHOLE
};
static HoverType g_hover_type = HOVER_NONE;
static int g_hover_idx = -1;
static int g_hover_wh_side = 0; // 0 = endpoint a, 1 = endpoint b

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
  case GLFW_KEY_F1:
    g_screenshot_pending = true;
    break;
  case GLFW_KEY_GRAVE_ACCENT:
    g_follow_mode = !g_follow_mode;
    break;
  case GLFW_KEY_U:
    g_uncapped = !g_uncapped;
    glfwSwapInterval(g_uncapped ? 0 : 1);
    break;
  case GLFW_KEY_A:
    for (auto &sh : g_ships)
      sh.selected = true;
    break;
  case GLFW_KEY_TAB: {
    glm::vec3 sum(0.0f);
    int count = 0;
    for (const auto &sh : g_ships)
      if (sh.selected) {
        sum += sh.pos;
        count++;
      }
    if (count > 0)
      cam.target = sum / (float)count;
    break;
  }
  case GLFW_KEY_F:
    g_formation =
        (FormationType)((g_formation + (shift ? FORMATION_COUNT - 1 : 1)) %
                        FORMATION_COUNT);
    break;
  case GLFW_KEY_0:
  case GLFW_KEY_1:
  case GLFW_KEY_2:
  case GLFW_KEY_3:
  case GLFW_KEY_4:
  case GLFW_KEY_5:
  case GLFW_KEY_6:
  case GLFW_KEY_7:
  case GLFW_KEY_8:
  case GLFW_KEY_9: {
    int g = key - GLFW_KEY_0;
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    if (ctrl) {
      g_groups[g].clear();
      for (int i = 0; i < (int)g_ships.size(); i++)
        if (g_ships[i].selected)
          g_groups[g].push_back(i);
      g_last_group_key = -1;
    } else {
      double now = glfwGetTime();
      if (g == g_last_group_key && (now - g_last_group_time) < 0.3) {
        // Double-press: move 3D cursor to average position of the group
        glm::vec3 sum(0.0f);
        for (int i : g_groups[g])
          sum += g_ships[i].pos;
        if (!g_groups[g].empty())
          cam.target = sum / (float)g_groups[g].size();
        g_last_group_key = -1;
      } else {
        for (auto &sh : g_ships)
          sh.selected = false;
        for (int i : g_groups[g])
          g_ships[i].selected = true;
        g_last_group_key = g;
        g_last_group_time = now;
      }
    }
    break;
  }
  case GLFW_KEY_P:
    g_paused = !g_paused;
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

// Checks all wormhole pairs for a shorter path than going direct.
static std::vector<glm::vec3> compute_route(glm::vec3 from, glm::vec3 to) {
  float best = glm::distance(from, to);
  int best_wh = -1;
  bool enter_a = true;
  for (int i = 0; i < NUM_WORMHOLE_PAIRS; i++) {
    float da = glm::distance(from, WORMHOLE_PAIRS[i].a) +
               glm::distance(WORMHOLE_PAIRS[i].b, to);
    float db = glm::distance(from, WORMHOLE_PAIRS[i].b) +
               glm::distance(WORMHOLE_PAIRS[i].a, to);
    if (da < best) {
      best = da;
      best_wh = i;
      enter_a = true;
    }
    if (db < best) {
      best = db;
      best_wh = i;
      enter_a = false;
    }
  }
  std::vector<glm::vec3> route;
  if (best_wh >= 0)
    route.push_back(enter_a ? WORMHOLE_PAIRS[best_wh].a
                            : WORMHOLE_PAIRS[best_wh].b);
  return route;
}

static void mouse_button_cb(GLFWwindow *win, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    std::vector<Spaceship *> sel;
    for (auto &sh : g_ships)
      if (sh.selected)
        sel.push_back(&sh);
    if (!sel.empty()) {
      // Generate a unique formation ID for this order batch (non-shift only)
      static int s_next_fid = 1;
      int fid = 0;
      if (!shift) {
        fid = s_next_fid++;
        for (auto *sh : sel)
          sh->formation_id = fid;
      }
      (void)fid;
      auto assign = [shift](Spaceship *sh, glm::vec3 target) {
        if (shift && sh->has_move_target) {
          sh->pending_targets.push_back(target);
        } else {
          sh->pending_targets.clear();
          sh->has_move_target = true;
          sh->move_target = target;
          sh->waypoints = compute_route(sh->pos, target);
        }
      };
      // Compute camera right/up basis for all formation types
      float cx = cosf(cam.pitch) * sinf(cam.yaw);
      float cy = sinf(cam.pitch);
      float cz = cosf(cam.pitch) * cosf(cam.yaw);
      glm::vec3 fwd = glm::normalize(glm::vec3(cx, cy, cz));
      glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
      glm::vec3 up = glm::cross(right, fwd);
      float spacing = cam.dist * 0.02f;
      int n = (int)sel.size();

      if (g_formation == FORMATION_HEX) {
        int idx = 0;
        for (int ring = 0; idx < n; ring++) {
          int slots = (ring == 0) ? 1 : ring * 6;
          for (int i = 0; i < slots && idx < n; i++, idx++) {
            float angle = (ring == 0) ? 0.0f : 2.0f * (float)M_PI * i / slots;
            glm::vec3 offset =
                (right * cosf(angle) + up * sinf(angle)) * (ring * spacing);
            assign(sel[idx], cam.target + offset);
          }
        }
      } else if (g_formation == FORMATION_LINE) {
        float total = (n - 1) * spacing;
        for (int i = 0; i < n; i++)
          assign(sel[i], cam.target + right * ((i * spacing) - total * 0.5f));
      } else if (g_formation == FORMATION_WEDGE) {
        assign(sel[0], cam.target);
        for (int i = 1; i < n; i++) {
          int pair = (i + 1) / 2;
          float side = (i % 2 == 1) ? 1.0f : -1.0f;
          assign(sel[i], cam.target + right * (side * pair * spacing) -
                             up * (pair * spacing));
        }
      } else if (g_formation == FORMATION_WALL) {
        int cols = (int)ceilf(sqrtf((float)n));
        int rows = (n + cols - 1) / cols;
        for (int i = 0; i < n; i++) {
          glm::vec3 offset =
              right * ((i % cols - (cols - 1) * 0.5f) * spacing) +
              up * ((i / cols - (rows - 1) * 0.5f) * spacing);
          assign(sel[i], cam.target + offset);
        }
      } else if (g_formation == FORMATION_BOX) {
        int side = (int)ceilf(cbrtf((float)n));
        for (int i = 0; i < n; i++) {
          glm::vec3 offset =
              right * ((i % side - (side - 1) * 0.5f) * spacing) +
              up * (((i / side) % side - (side - 1) * 0.5f) * spacing) +
              fwd * ((i / (side * side) - (side - 1) * 0.5f) * spacing);
          assign(sel[i], cam.target + offset);
        }
      } else if (g_formation == FORMATION_SPHERE) {
        float radius = spacing * sqrtf((float)n / (float)M_PI);
        float golden = (float)M_PI * (3.0f - sqrtf(5.0f));
        for (int i = 0; i < n; i++) {
          float y = (n > 1) ? (1.0f - 2.0f * i / (n - 1)) : 0.0f;
          float r = sqrtf(fmaxf(0.0f, 1.0f - y * y));
          float theta = golden * i;
          glm::vec3 dir =
              right * (cosf(theta) * r) + up * y + fwd * (sinf(theta) * r);
          assign(sel[i], cam.target + dir * radius);
        }
      } else if (g_formation == FORMATION_RANDOM) {
        float radius = spacing * sqrtf((float)n / (float)M_PI);
        for (int i = 0; i < n; i++) {
          glm::vec3 offset;
          do {
            float rx = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
            float ry = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
            float rz = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
            offset = glm::vec3(rx, ry, rz) * radius;
          } while (glm::length(offset) > radius);
          assign(sel[i], cam.target + offset);
        }
      }
    }
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    g_rmb_down = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    mmb_down = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    if (action == GLFW_PRESS) {
      g_lmb_down = true;
      g_drag_active = false;
      glfwGetCursorPos(win, &g_drag_start_x, &g_drag_start_y);
      g_drag_cur_x = g_drag_start_x;
      g_drag_cur_y = g_drag_start_y;
    } else {
      if (!g_drag_active) {
        g_pick_pending = true;
        g_pick_x = g_drag_start_x;
        g_pick_y = g_drag_start_y;
      } else {
        g_drag_select_pending = true;
      }
      g_lmb_down = false;
      g_drag_active = false;
    }
  }
}

static void cursor_pos_cb(GLFWwindow *win, double mx, double my) {
  double dx = mx - last_mx;
  double dy = my - last_my;
  last_mx = mx;
  last_my = my;

  if (g_lmb_down) {
    g_drag_cur_x = mx;
    g_drag_cur_y = my;
    float ddx = (float)(mx - g_drag_start_x);
    float ddy = (float)(my - g_drag_start_y);
    if (sqrtf(ddx * ddx + ddy * ddy) > 5.0f)
      g_drag_active = true;
  }

  if (g_rmb_down) {
    // Rotate selected ships' move targets around cam.target
    const float sensitivity = 0.005f;
    float cx = cosf(cam.pitch) * sinf(cam.yaw);
    float cy = sinf(cam.pitch);
    float cz = cosf(cam.pitch) * cosf(cam.yaw);
    glm::vec3 fwd = glm::normalize(glm::vec3(cx, cy, cz));
    glm::vec3 cam_right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
    glm::vec3 cam_up = glm::cross(cam_right, fwd);
    glm::mat4 rot =
        glm::rotate(glm::mat4(1.0f), (float)dx * sensitivity, cam_up) *
        glm::rotate(glm::mat4(1.0f), (float)dy * sensitivity, cam_right);
    for (auto &sh : g_ships) {
      if (!sh.selected || !sh.has_move_target)
        continue;
      glm::vec3 rel = sh.move_target - cam.target;
      sh.move_target = cam.target + glm::vec3(rot * glm::vec4(rel, 0.0f));
      sh.waypoints.clear();
    }
  }

  if (!mmb_down)
    return;

  bool shift = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
               glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

  if (shift) {
    // Pan: move target in camera-local right/up plane
    g_follow_mode = false;
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

    target_yaw = cam.yaw;
    target_pitch = cam.pitch;
  }
}

int main(int argc, char **argv) {
  bool fullscreen = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--uncap-framerate") == 0)
      g_uncapped = true;
    if (strcmp(argv[i], "--fullscreen") == 0)
      fullscreen = true;
    if (strcmp(argv[i], "--help") == 0) {
      printf("Usage: %s [options]\n"
             "  --fullscreen        Start in fullscreen mode\n"
             "  --uncap-framerate   Disable vsync\n"
             "  --help              Show this help\n",
             argv[0]);
      return 0;
    }
  }
  if (!glfwInit()) {
    fprintf(stderr, "Failed to init GLFW\n");
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWmonitor *monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
  int win_w = 800, win_h = 600;
  if (fullscreen) {
    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    win_w = mode->width;
    win_h = mode->height;
  }
  GLFWwindow *win =
      glfwCreateWindow(win_w, win_h, "Space Game", monitor, nullptr);
  if (!win) {
    fprintf(stderr, "Failed to create window\n");
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(win);
  glfwSwapInterval(g_uncapped ? 0 : 1);
  glfwSetKeyCallback(win, key_cb);
  glfwSetMouseButtonCallback(win, mouse_button_cb);
  glfwSetCursorPosCallback(win, cursor_pos_cb);
  glfwSetScrollCallback(win, scroll_cb);

  glfwGetCursorPos(win, &last_mx, &last_my);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to init GLEW\n");
    return 1;
  }

  text_init(18);
  g_text_prog = build_program(text_vert_src, text_frag_src);

  // Load test-image.png
  GLuint img_tex = 0;
  int img_w = 0, img_h = 0;
  {
    int channels;
    stbi_set_flip_vertically_on_load(1);
    unsigned char *data =
        stbi_load("test-image.png", &img_w, &img_h, &channels, 4);
    if (data) {
      glGenTextures(1, &img_tex);
      glBindTexture(GL_TEXTURE_2D, img_tex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_w, img_h, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, data);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      stbi_image_free(data);
    } else {
      fprintf(stderr, "Failed to load test-image.png: %s\n",
              stbi_failure_reason());
    }
  }
  GLuint img_prog = build_program(img_vert_src, img_frag_src);

  // Quad VAO for image (6 verts: pos.xy + uv.xy)
  GLuint img_vao, img_vbo;
  glGenVertexArrays(1, &img_vao);
  glGenBuffers(1, &img_vbo);
  glBindVertexArray(img_vao);
  glBindBuffer(GL_ARRAY_BUFFER, img_vbo);
  glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  GLuint prog = build_program(vert_src, frag_src);
  GLint mvp_loc = glGetUniformLocation(prog, "mvp");
  GLint color_loc = glGetUniformLocation(prog, "u_color");
  GLint use_vc_loc = glGetUniformLocation(prog, "u_use_vertex_color");
  GLint point_size_loc = glGetUniformLocation(prog, "u_point_size");
  GLint is_star_loc = glGetUniformLocation(prog, "u_is_star");
  GLint falloff_loc = glGetUniformLocation(prog, "u_glow_falloff");
  GLint alpha_loc = glGetUniformLocation(prog, "u_alpha");
  glUseProgram(prog);
  glUniform1f(alpha_loc, 1.0f);

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
                             r * sinf(a), 0.18f, 0.18f, 0.25f});
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
      r += 0.15f + (rand() / (float)RAND_MAX) * 0.35f;
    }
  }

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

  auto make_ship = [&](glm::vec3 pos, ShipShape shape) {
    Spaceship s;
    s.pos = pos;
    s.shape = shape;
    glGenVertexArrays(1, &s.vao);
    glGenBuffers(1, &s.vbo);
    glBindVertexArray(s.vao);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * 6 * sizeof(float), nullptr,
                 GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    g_ships.push_back(s);
  };

  // 11 triangle ships in a tight formation
  static const glm::vec3 tri_positions[] = {
      {0.00f, 0.00f, 0.00f},    {0.04f, 0.01f, 0.02f},
      {-0.03f, 0.02f, -0.01f},  {0.02f, -0.03f, 0.04f},
      {-0.01f, 0.04f, -0.03f},  {0.05f, 0.00f, -0.02f},
      {-0.04f, -0.01f, 0.03f},  {0.01f, 0.03f, 0.05f},
      {-0.02f, -0.04f, -0.04f}, {0.03f, 0.02f, -0.05f},
      {-0.05f, 0.03f, 0.01f},
  };
  for (const auto &p : tri_positions)
    make_ship(p, SHIP_TRIANGLE);

  // 100 square ships scattered in a wider cloud around the formation
  srand(555);
  for (int i = 0; i < 100; i++) {
    float x = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f) * 0.3f;
    float y = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f) * 0.3f;
    float z = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f) * 0.3f;
    make_ship({x, y, z}, SHIP_SQUARE);
  }

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

  // Milky Way: dense band of faint glowing points around a galactic plane
  static constexpr int NUM_MW = 6000;
  // Galactic basis: pole and two in-plane axes
  glm::vec3 gal_pole = glm::normalize(glm::vec3(0.2f, 0.85f, 0.5f));
  glm::vec3 gal_x = glm::normalize(glm::cross(gal_pole, glm::vec3(1, 0, 0)));
  glm::vec3 gal_y = glm::cross(gal_pole, gal_x);
  glm::vec3 gal_center = glm::normalize(glm::vec3(0.8f, -0.3f, 0.5f));

  std::vector<float> mw_verts;
  mw_verts.reserve(NUM_MW * 6);
  srand(9999);
  for (int i = 0; i < NUM_MW; i++) {
    float phi = (rand() / (float)RAND_MAX) * 2.0f * (float)M_PI;
    // Box-Muller Gaussian for latitude offset from galactic plane
    float u1 = rand() / (float)RAND_MAX + 1e-6f;
    float u2 = rand() / (float)RAND_MAX;
    float lat = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2) *
                glm::radians(12.0f);
    float cl = cosf(lat), sl = sinf(lat);
    glm::vec3 dir = glm::normalize(
        cl * (cosf(phi) * gal_x + sinf(phi) * gal_y) + sl * gal_pole);
    // Brighter toward the galactic center
    float toward_center = 0.5f + 0.5f * glm::dot(dir, gal_center);
    float b = (0.004f + 0.016f * toward_center) *
              powf(rand() / (float)RAND_MAX, 1.2f);
    mw_verts.insert(mw_verts.end(),
                    {dir.x, dir.y, dir.z, b * 0.75f, b * 0.85f, b});
  }

  GLuint mw_vao, mw_vbo;
  glGenVertexArrays(1, &mw_vao);
  glGenBuffers(1, &mw_vbo);
  glBindVertexArray(mw_vao);
  glBindBuffer(GL_ARRAY_BUFFER, mw_vbo);
  glBufferData(GL_ARRAY_BUFFER, mw_verts.size() * sizeof(float),
               mw_verts.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);

  // Drag selection box: 4 corners, updated dynamically each frame
  GLuint drag_vao, drag_vbo;
  glGenVertexArrays(1, &drag_vao);
  glGenBuffers(1, &drag_vbo);
  glBindVertexArray(drag_vao);
  glBindBuffer(GL_ARRAY_BUFFER, drag_vbo);
  glBufferData(GL_ARRAY_BUFFER, 4 * 6 * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Move-order lines: 2 vertices per ship (ship pos → move target)
  GLuint move_vao, move_vbo;
  glGenVertexArrays(1, &move_vao);
  glGenBuffers(1, &move_vbo);
  glBindVertexArray(move_vao);
  glBindBuffer(GL_ARRAY_BUFFER, move_vbo);
  glBufferData(GL_ARRAY_BUFFER, g_ships.size() * 10 * 6 * sizeof(float),
               nullptr, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Wormhole connector lines: 2 verts per pair (pos+color)
  std::vector<float> wh_line_verts;
  for (const auto &wp : WORMHOLE_PAIRS) {
    wh_line_verts.insert(wh_line_verts.end(),
                         {wp.a.x, wp.a.y, wp.a.z, 0.5f, 0.2f, 0.9f});
    wh_line_verts.insert(wh_line_verts.end(),
                         {wp.b.x, wp.b.y, wp.b.z, 0.5f, 0.2f, 0.9f});
  }
  GLuint wh_vao, wh_vbo;
  glGenVertexArrays(1, &wh_vao);
  glGenBuffers(1, &wh_vbo);
  glBindVertexArray(wh_vao);
  glBindBuffer(GL_ARRAY_BUFFER, wh_vbo);
  glBufferData(GL_ARRAY_BUFFER, wh_line_verts.size() * sizeof(float),
               wh_line_verts.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  double prev_time = glfwGetTime();

  while (!glfwWindowShouldClose(win)) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(win, GLFW_TRUE);

    double now = glfwGetTime();
    float dt = (float)(now - prev_time);
    prev_time = now;

    if (!g_paused) {
      // Move ships toward their assigned targets (via waypoints if routed
      // through wormhole)
      static constexpr float SHIP_SPEED = 0.4f;
      for (auto &sh : g_ships) {
        if (!sh.has_move_target)
          continue;
        glm::vec3 dest =
            sh.waypoints.empty() ? sh.move_target : sh.waypoints.front();
        glm::vec3 delta = dest - sh.pos;
        float dist = glm::length(delta);
        float step = SHIP_SPEED * dt * g_timescale;
        if (dist <= step) {
          sh.pos = dest;
          if (!sh.waypoints.empty()) {
            // Arrived at a waypoint — check for wormhole teleport then pop it
            for (const auto &wp : WORMHOLE_PAIRS) {
              if (glm::distance(sh.pos, wp.a) < 0.001f) {
                sh.pos = wp.b;
                break;
              }
              if (glm::distance(sh.pos, wp.b) < 0.001f) {
                sh.pos = wp.a;
                break;
              }
            }
            sh.waypoints.erase(sh.waypoints.begin());
          } else if (!sh.pending_targets.empty()) {
            // Start next queued leg
            glm::vec3 next = sh.pending_targets.front();
            sh.pending_targets.erase(sh.pending_targets.begin());
            sh.move_target = next;
            sh.waypoints = compute_route(sh.pos, next);
          } else {
            sh.has_move_target = false;
          }
        } else {
          sh.pos += (delta / dist) * step;
        }
      }
    }

    // Follow mode: keep 3D cursor at average position of selected ships
    if (g_follow_mode) {
      glm::vec3 sum(0.0f);
      int count = 0;
      for (const auto &sh : g_ships)
        if (sh.selected) {
          sum += sh.pos;
          count++;
        }
      if (count > 0)
        cam.target = sum / (float)count;
    }

    // Smoothly interpolate camera toward target orientation
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

    // Hover detection: find the closest object to the current cursor position
    {
      auto sdist = [&](glm::vec3 world) -> float {
        glm::vec4 clip = proj * view * glm::vec4(world, 1.0f);
        if (clip.w <= 0.0f)
          return 1e9f;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float sx = (ndc.x * 0.5f + 0.5f) * w;
        float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * h;
        float dx = (float)last_mx - sx, dy = (float)last_my - sy;
        return sqrtf(dx * dx + dy * dy);
      };
      float best = 30.0f;
      g_hover_type = HOVER_NONE;
      g_hover_idx = -1;
      for (int i = 0; i < (int)(sizeof(LOCAL_STARS) / sizeof(LOCAL_STARS[0]));
           i++) {
        float d = sdist(LOCAL_STARS[i].pos);
        if (d < best) {
          best = d;
          g_hover_type = HOVER_STAR;
          g_hover_idx = i;
        }
      }
      for (int i = 0; i < (int)g_planets.size(); i++) {
        const Planet &pl = g_planets[i];
        glm::vec3 pos =
            pl.star_pos + glm::vec3(pl.orbit_radius * cosf(pl.orbit_angle),
                                    pl.orbit_radius * sinf(pl.orbit_tilt) *
                                        sinf(pl.orbit_angle),
                                    pl.orbit_radius * sinf(pl.orbit_angle));
        float d = sdist(pos);
        if (d < best) {
          best = d;
          g_hover_type = HOVER_PLANET;
          g_hover_idx = i;
        }
      }
      for (int i = 0; i < (int)g_ships.size(); i++) {
        float d = sdist(g_ships[i].pos);
        if (d < best) {
          best = d;
          g_hover_type = HOVER_SHIP;
          g_hover_idx = i;
        }
      }
      for (int i = 0; i < NUM_WORMHOLE_PAIRS; i++) {
        float da = sdist(WORMHOLE_PAIRS[i].a);
        float db = sdist(WORMHOLE_PAIRS[i].b);
        if (da < best) {
          best = da;
          g_hover_type = HOVER_WORMHOLE;
          g_hover_idx = i;
          g_hover_wh_side = 0;
        }
        if (db < best) {
          best = db;
          g_hover_type = HOVER_WORMHOLE;
          g_hover_idx = i;
          g_hover_wh_side = 1;
        }
      }
    }

    // Picking: find the star or planet closest to a left-click
    if (g_pick_pending) {
      g_pick_pending = false;
      static const char *spectral[] = {
          "G (yellow)",       "B (blue-white)",   "M (red dwarf)",
          "A (white)",        "K (orange)",       "O (hot blue)",
          "F (yellow-white)", "K (orange giant)", "B (blue)",
          "M (red)",          "A (white)",
      };

      auto screen_dist = [&](glm::vec3 world) -> float {
        glm::vec4 clip = proj * view * glm::vec4(world, 1.0f);
        if (clip.w <= 0.0f)
          return 1e9f;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float sx = (ndc.x * 0.5f + 0.5f) * w;
        float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * h;
        float dx = (float)g_pick_x - sx, dy = (float)g_pick_y - sy;
        return sqrtf(dx * dx + dy * dy);
      };

      float best_d = 20.0f;
      int best_star = -1, best_planet = -1;
      int best_ship = -1;

      for (int i = 0; i < (int)(sizeof(LOCAL_STARS) / sizeof(LOCAL_STARS[0]));
           i++) {
        float d = screen_dist(LOCAL_STARS[i].pos);
        if (d < best_d) {
          best_d = d;
          best_star = i;
          best_planet = -1;
          best_ship = -1;
        }
      }
      for (int i = 0; i < (int)g_planets.size(); i++) {
        const Planet &pl = g_planets[i];
        glm::vec3 pos =
            pl.star_pos + glm::vec3(pl.orbit_radius * cosf(pl.orbit_angle),
                                    pl.orbit_radius * sinf(pl.orbit_tilt) *
                                        sinf(pl.orbit_angle),
                                    pl.orbit_radius * sinf(pl.orbit_angle));
        float d = screen_dist(pos);
        if (d < best_d) {
          best_d = d;
          best_planet = i;
          best_star = -1;
          best_ship = -1;
        }
      }
      for (int i = 0; i < (int)g_ships.size(); i++) {
        float d = screen_dist(g_ships[i].pos);
        if (d < best_d) {
          best_d = d;
          best_ship = i;
          best_star = -1;
          best_planet = -1;
        }
      }

      if (best_star >= 0) {
        const LocalStar &s = LOCAL_STARS[best_star];
        int n_planets = 0;
        for (const auto &pl : g_planets)
          if (pl.star_pos == s.pos)
            n_planets++;
        printf("Star %d\n", best_star);
        printf("  Type:     %s\n", spectral[best_star]);
        printf("  Position: (%.3f, %.3f, %.3f)\n", s.pos.x, s.pos.y, s.pos.z);
        printf("  Color:    (%.2f, %.2f, %.2f)\n", s.color.r, s.color.g,
               s.color.b);
        printf("  Planets:  %d\n", n_planets);
        fflush(stdout);
      } else if (best_ship >= 0) {
        int fid = g_ships[best_ship].formation_id;
        for (auto &sh : g_ships)
          sh.selected = (fid != 0 && sh.formation_id == fid);
        if (!g_ships[best_ship].selected)
          g_ships[best_ship].selected = true;
        const Spaceship &sh = g_ships[best_ship];
        printf("Spaceship %d\n", best_ship);
        printf("  Position: (%.3f, %.3f, %.3f)\n", sh.pos.x, sh.pos.y,
               sh.pos.z);
        fflush(stdout);
      } else if (best_planet >= 0) {
        const Planet &pl = g_planets[best_planet];
        int parent = -1;
        for (int i = 0; i < (int)(sizeof(LOCAL_STARS) / sizeof(LOCAL_STARS[0]));
             i++)
          if (LOCAL_STARS[i].pos == pl.star_pos) {
            parent = i;
            break;
          }
        const char *zone = pl.orbit_radius < 0.5f   ? "Rocky"
                           : pl.orbit_radius < 1.1f ? "Gas giant"
                                                    : "Ice giant";
        glm::vec3 pos =
            pl.star_pos + glm::vec3(pl.orbit_radius * cosf(pl.orbit_angle),
                                    pl.orbit_radius * sinf(pl.orbit_tilt) *
                                        sinf(pl.orbit_angle),
                                    pl.orbit_radius * sinf(pl.orbit_angle));
        printf("Planet %d (Star %d)\n", best_planet, parent);
        printf("  Zone:         %s\n", zone);
        printf("  Position:     (%.3f, %.3f, %.3f)\n", pos.x, pos.y, pos.z);
        printf("  Orbit radius: %.3f\n", pl.orbit_radius);
        printf("  Orbit period: %.2f s\n", pl.orbit_period);
        printf("  Orbit tilt:   %.1f deg\n", glm::degrees(pl.orbit_tilt));
        fflush(stdout);
      } else {
        // Clicked empty space — deselect all
        for (auto &sh : g_ships)
          sh.selected = false;
      }
    }

    // Drag selection: select ships whose screen position falls inside the box
    if (g_drag_select_pending) {
      g_drag_select_pending = false;
      float bx0 = (float)fmin(g_drag_start_x, g_drag_cur_x);
      float bx1 = (float)fmax(g_drag_start_x, g_drag_cur_x);
      float by0 = (float)fmin(g_drag_start_y, g_drag_cur_y);
      float by1 = (float)fmax(g_drag_start_y, g_drag_cur_y);
      for (auto &sh : g_ships) {
        glm::vec4 clip = proj * view * glm::vec4(sh.pos, 1.0f);
        if (clip.w <= 0.0f) {
          sh.selected = false;
          continue;
        }
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float sx = (ndc.x * 0.5f + 0.5f) * w;
        float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * h;
        sh.selected = (sx >= bx0 && sx <= bx1 && sy >= by0 && sy <= by1);
      }
    }

    // Rotation-only view for stars (strip translation so they're at infinity)
    glm::mat4 view_rot = glm::mat4(glm::mat3(view));

    glUseProgram(prog);
    glUniform1f(point_size_loc, 1.0f);

    // Draw Milky Way band (before background stars, additive glow)
    glDepthMask(GL_FALSE);
    glUniform1i(use_vc_loc, 1);
    glUniform1i(is_star_loc, 1);
    glUniform1f(point_size_loc, 120.0f);
    glUniform1f(falloff_loc, 1.0f);
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(proj * view_rot));
    glBindVertexArray(mw_vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDrawArrays(GL_POINTS, 0, NUM_MW);
    glDisable(GL_BLEND);
    glUniform1i(is_star_loc, 0);
    glDepthMask(GL_TRUE);

    // Draw stars (depth writes off so they're always behind everything)
    glDepthMask(GL_FALSE);
    glUniform1i(use_vc_loc, 1);
    glUniform1f(point_size_loc, 1.0f);
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

    // Draw wormholes: connector lines + two-layer glow per endpoint
    {
      // Connector lines
      glUniform1i(use_vc_loc, 1);
      glUniform1i(is_star_loc, 0);
      glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
      glUniform1f(alpha_loc, 0.4f);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBindVertexArray(wh_vao);
      glLineWidth(1.0f);
      glDrawArrays(GL_LINES, 0, NUM_WORMHOLE_PAIRS * 2);
      glDisable(GL_BLEND);
      glUniform1f(alpha_loc, 1.0f);

      // Glow layers for each endpoint
      glUniform1i(use_vc_loc, 0);
      glUniform1i(is_star_loc, 1);
      glBindVertexArray(sun_vao);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glDepthMask(GL_FALSE);
      for (const auto &wp : WORMHOLE_PAIRS) {
        for (int e = 0; e < 2; e++) {
          glm::vec3 p = (e == 0) ? wp.a : wp.b;
          glm::mat4 wh_mvp = mvp * glm::translate(glm::mat4(1.0f), p);
          glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(wh_mvp));
          // Outer halo
          glUniform1f(point_size_loc, 90.0f);
          glUniform1f(falloff_loc, 3.5f);
          glUniform3f(color_loc, 0.35f, 0.1f, 0.8f);
          glDrawArrays(GL_POINTS, 0, 1);
          // Inner core
          glUniform1f(point_size_loc, 28.0f);
          glUniform1f(falloff_loc, 7.0f);
          glUniform3f(color_loc, 0.75f, 0.5f, 1.0f);
          glDrawArrays(GL_POINTS, 0, 1);
        }
      }
      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
      glUniform1i(is_star_loc, 0);
    }

    // Draw all planets and orbit lines
    glUniform1i(use_vc_loc, 1);
    glUniform1i(is_star_loc, 0);
    for (int pi = 0; pi < (int)g_planets.size(); pi++) {
      Planet &pl = g_planets[pi];
      pl.orbit_angle +=
          g_paused ? 0.0f
                   : dt * g_timescale * (2.0f * (float)M_PI / pl.orbit_period);
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

      bool orbit_hovered = (g_hover_type == HOVER_PLANET && g_hover_idx == pi);
      glm::mat4 orbit_mvp = mvp * glm::translate(glm::mat4(1.0f), pl.star_pos);
      glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(orbit_mvp));
      glBindVertexArray(pl.orbit_vao);
      glLineWidth(orbit_hovered ? 2.0f : 1.0f);
      if (orbit_hovered) {
        glUniform1i(use_vc_loc, 0);
        glUniform3f(color_loc, 0.45f, 0.45f, 0.75f);
      }
      glDrawArrays(GL_LINE_LOOP, 0, ORBIT_SEGS);
      if (orbit_hovered)
        glUniform1i(use_vc_loc, 1);
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

    glm::vec3 cam_right = glm::vec3(view[0][0], view[1][0], view[2][0]);
    glm::vec3 cam_up = glm::vec3(view[0][1], view[1][1], view[2][1]);

    // Draw spaceships: billboard triangles always facing the camera.
    {
      float s = cam.dist * 0.012f;
      glUniform1i(use_vc_loc, 1);
      glUniform1i(is_star_loc, 0);
      glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
      for (auto &ship : g_ships) {
        glBindVertexArray(ship.vao);
        glBindBuffer(GL_ARRAY_BUFFER, ship.vbo);
        // Selected: yellow fill / dark-yellow outline. Unselected: cyan /
        // dark-teal.
        float fr = ship.selected ? 1.00f : 0.20f;
        float fg = ship.selected ? 1.00f : 1.00f;
        float fb = ship.selected ? 0.30f : 0.90f;
        float or_ = ship.selected ? 0.50f : 0.05f;
        float og = ship.selected ? 0.50f : 0.45f;
        float ob = ship.selected ? 0.10f : 0.40f;
        if (ship.shape == SHIP_TRIANGLE) {
          glm::vec3 v0 = ship.pos + s * (cam_up * 0.6f);
          glm::vec3 v1 = ship.pos + s * (-cam_right * 0.5f - cam_up * 0.3f);
          glm::vec3 v2 = ship.pos + s * (cam_right * 0.5f - cam_up * 0.3f);
          float sv[] = {
              v0.x, v0.y, v0.z, fr,   fg,   fb,   v1.x, v1.y, v1.z,
              fr,   fg,   fb,   v2.x, v2.y, v2.z, fr,   fg,   fb,
          };
          glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(sv), sv);
          glDrawArrays(GL_TRIANGLES, 0, 3);
          float ov[] = {
              v0.x, v0.y, v0.z, or_,  og,   ob,   v1.x, v1.y, v1.z,
              or_,  og,   ob,   v2.x, v2.y, v2.z, or_,  og,   ob,
          };
          glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ov), ov);
          glLineWidth(2.0f);
          glDrawArrays(GL_LINE_LOOP, 0, 3);
        } else {
          float ss = s * 0.15f;
          glm::vec3 v0 = ship.pos + ss * (-cam_right + cam_up);
          glm::vec3 v1 = ship.pos + ss * (cam_right + cam_up);
          glm::vec3 v2 = ship.pos + ss * (cam_right - cam_up);
          glm::vec3 v3 = ship.pos + ss * (-cam_right - cam_up);
          float sv[] = {
              v0.x, v0.y, v0.z, fr, fg, fb, v1.x, v1.y, v1.z, fr, fg, fb,
              v2.x, v2.y, v2.z, fr, fg, fb, v0.x, v0.y, v0.z, fr, fg, fb,
              v2.x, v2.y, v2.z, fr, fg, fb, v3.x, v3.y, v3.z, fr, fg, fb,
          };
          glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(sv), sv);
          glDrawArrays(GL_TRIANGLES, 0, 6);
          float ov[] = {
              v0.x, v0.y, v0.z, or_, og, ob, v1.x, v1.y, v1.z, or_, og, ob,
              v2.x, v2.y, v2.z, or_, og, ob, v3.x, v3.y, v3.z, or_, og, ob,
          };
          glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ov), ov);
          glLineWidth(2.0f);
          glDrawArrays(GL_LINE_LOOP, 0, 4);
        }
      }
    }

    // Draw move-order lines (ship -> assigned target) + diamond indicators
    {
      std::vector<float> mv;  // line vertices
      std::vector<float> ind; // indicator vertices

      float is = cam.dist * 0.004f; // indicator half-size
      for (const auto &sh : g_ships) {
        if (!sh.has_move_target)
          continue;
        // Draw route: ship -> wormhole stops -> move_target -> pending queue
        auto push_line = [&](glm::vec3 a, glm::vec3 b2) {
          mv.insert(mv.end(), {a.x, a.y, a.z, 0.25f, 0.25f, 0.06f});
          mv.insert(mv.end(), {b2.x, b2.y, b2.z, 0.25f, 0.25f, 0.06f});
        };
        auto push_diamond = [&](glm::vec3 p) {
          glm::vec3 t = p + cam_up * is, r = p + cam_right * is;
          glm::vec3 b = p - cam_up * is, l = p - cam_right * is;
          ind.insert(ind.end(), {t.x, t.y, t.z, 0.3f, 0.9f, 1.0f, r.x, r.y, r.z,
                                 0.3f, 0.9f, 1.0f});
          ind.insert(ind.end(), {r.x, r.y, r.z, 0.3f, 0.9f, 1.0f, b.x, b.y, b.z,
                                 0.3f, 0.9f, 1.0f});
          ind.insert(ind.end(), {b.x, b.y, b.z, 0.3f, 0.9f, 1.0f, l.x, l.y, l.z,
                                 0.3f, 0.9f, 1.0f});
          ind.insert(ind.end(), {l.x, l.y, l.z, 0.3f, 0.9f, 1.0f, t.x, t.y, t.z,
                                 0.3f, 0.9f, 1.0f});
        };
        glm::vec3 prev = sh.pos;
        for (const auto &wp : sh.waypoints) {
          push_line(prev, wp);
          prev = wp;
        }
        push_line(prev, sh.move_target);
        push_diamond(sh.move_target);
        prev = sh.move_target;
        for (const auto &pt : sh.pending_targets) {
          push_line(prev, pt);
          push_diamond(pt);
          prev = pt;
        }
      }
      if (!mv.empty()) {
        glUseProgram(prog);
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform1i(use_vc_loc, 1);
        glUniform1i(is_star_loc, 0);
        glUniform1f(point_size_loc, 1.0f);
        glBindVertexArray(move_vao);
        glBindBuffer(GL_ARRAY_BUFFER, move_vbo);
        // Pack lines then indicators contiguously
        size_t line_bytes = mv.size() * sizeof(float);
        size_t ind_bytes = ind.size() * sizeof(float);
        glBufferSubData(GL_ARRAY_BUFFER, 0, line_bytes, mv.data());
        glBufferSubData(GL_ARRAY_BUFFER, line_bytes, ind_bytes, ind.data());
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniform1f(alpha_loc, 0.35f);
        glLineWidth(1.0f);
        glDrawArrays(GL_LINES, 0, (GLsizei)(mv.size() / 6));
        glUniform1f(alpha_loc, 0.6f);
        glLineWidth(1.5f);
        glDrawArrays(GL_LINES, (GLsizei)(mv.size() / 6),
                     (GLsizei)(ind.size() / 6));
        glUniform1f(alpha_loc, 1.0f);
        glDisable(GL_BLEND);
      }
    }

    // Draw drag selection box
    if (g_drag_active) {
      float x0 = (float)g_drag_start_x, y0 = (float)g_drag_start_y;
      float x1 = (float)g_drag_cur_x, y1 = (float)g_drag_cur_y;
      float dv[] = {
          x0, y0, 0.0f, 1.0f, 1.0f, 1.0f, x1, y0, 0.0f, 1.0f, 1.0f, 1.0f,
          x1, y1, 0.0f, 1.0f, 1.0f, 1.0f, x0, y1, 0.0f, 1.0f, 1.0f, 1.0f,
      };
      glm::mat4 screen_mvp =
          glm::ortho(0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
      glUseProgram(prog);
      glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(screen_mvp));
      glUniform1i(use_vc_loc, 1);
      glUniform1i(is_star_loc, 0);
      glUniform1f(point_size_loc, 1.0f);
      glBindVertexArray(drag_vao);
      glBindBuffer(GL_ARRAY_BUFFER, drag_vbo);
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(dv), dv);
      glLineWidth(1.0f);
      glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    // FPS counter: update display value once per second
    static int fps_frames = 0;
    static float fps_elapsed = 0.0f;
    static int fps_display = 0;
    fps_frames++;
    fps_elapsed += dt;
    if (fps_elapsed >= 0.1f) {
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

    if (g_paused) {
      const char *ps = "PAUSED";
      text_draw(ps, w - text_width(ps) - 10.0f, 10.0f, {1.0f, 0.8f, 0.2f}, w,
                h);
    } else {
      char ts_buf[32];
      snprintf(ts_buf, sizeof(ts_buf), "%gx", g_timescale);
      text_draw(ts_buf, w - text_width(ts_buf) - 10.0f, 10.0f,
                {0.8f, 0.8f, 0.8f}, w, h);
    }

    text_draw(FORMATION_NAMES[g_formation], 10.0f, h - 10.0f - g_font_size,
              {0.8f, 0.8f, 0.2f}, w, h);
    if (g_follow_mode)
      text_draw("FOLLOW", 10.0f, h - 10.0f - (g_font_size + 4) * 2,
                {0.3f, 0.9f, 1.0f}, w, h);

    // Hover info panel — left side, vertically centered
    if (g_hover_type != HOVER_NONE) {
      static const char *spectral[] = {
          "G (yellow)",       "B (blue-white)",   "M (red dwarf)",
          "A (white)",        "K (orange)",       "O (hot blue)",
          "F (yellow-white)", "K (orange giant)", "B (blue)",
          "M (red)",          "A (white)",
      };
      char lines[8][80];
      int nlines = 0;
      float lh = g_font_size + 4.0f;

      if (g_hover_type == HOVER_STAR) {
        const LocalStar &s = LOCAL_STARS[g_hover_idx];
        int np = 0;
        for (const auto &pl : g_planets)
          if (pl.star_pos == s.pos)
            np++;
        snprintf(lines[nlines++], 80, "Star %d", g_hover_idx);
        snprintf(lines[nlines++], 80, "Type:     %s", spectral[g_hover_idx]);
        snprintf(lines[nlines++], 80, "Pos:      %.2f  %.2f  %.2f", s.pos.x,
                 s.pos.y, s.pos.z);
        snprintf(lines[nlines++], 80, "Planets:  %d", np);
      } else if (g_hover_type == HOVER_PLANET) {
        const Planet &pl = g_planets[g_hover_idx];
        int parent = -1;
        for (int i = 0; i < (int)(sizeof(LOCAL_STARS) / sizeof(LOCAL_STARS[0]));
             i++)
          if (LOCAL_STARS[i].pos == pl.star_pos) {
            parent = i;
            break;
          }
        const char *zone = pl.orbit_radius < 0.5f   ? "Rocky"
                           : pl.orbit_radius < 1.1f ? "Gas giant"
                                                    : "Ice giant";
        snprintf(lines[nlines++], 80, "Planet %d  (Star %d)", g_hover_idx,
                 parent);
        snprintf(lines[nlines++], 80, "Zone:     %s", zone);
        snprintf(lines[nlines++], 80, "Radius:   %.3f", pl.orbit_radius);
        snprintf(lines[nlines++], 80, "Period:   %.2f s", pl.orbit_period);
        snprintf(lines[nlines++], 80, "Tilt:     %.1f deg",
                 glm::degrees(pl.orbit_tilt));
      } else if (g_hover_type == HOVER_SHIP) {
        const Spaceship &sh = g_ships[g_hover_idx];
        snprintf(lines[nlines++], 80, "Ship %d", g_hover_idx);
        snprintf(lines[nlines++], 80, "Pos:      %.2f  %.2f  %.2f", sh.pos.x,
                 sh.pos.y, sh.pos.z);
        snprintf(lines[nlines++], 80, "Selected: %s",
                 sh.selected ? "yes" : "no");
        if (sh.formation_id)
          snprintf(lines[nlines++], 80, "Formation: %d", sh.formation_id);
        if (sh.has_move_target)
          snprintf(lines[nlines++], 80, "Target:   %.2f  %.2f  %.2f",
                   sh.move_target.x, sh.move_target.y, sh.move_target.z);
        if (!sh.pending_targets.empty())
          snprintf(lines[nlines++], 80, "Waypoints: %d queued",
                   (int)sh.pending_targets.size());
      } else if (g_hover_type == HOVER_WORMHOLE) {
        const WormholePair &wp = WORMHOLE_PAIRS[g_hover_idx];
        const glm::vec3 &here = g_hover_wh_side == 0 ? wp.a : wp.b;
        const glm::vec3 &there = g_hover_wh_side == 0 ? wp.b : wp.a;
        snprintf(lines[nlines++], 80, "Wormhole %d%s", g_hover_idx,
                 g_hover_wh_side == 0 ? "A" : "B");
        snprintf(lines[nlines++], 80, "Pos:      %.2f  %.2f  %.2f", here.x,
                 here.y, here.z);
        snprintf(lines[nlines++], 80, "Exit:     %.2f  %.2f  %.2f", there.x,
                 there.y, there.z);
        float dist = glm::distance(wp.a, wp.b);
        snprintf(lines[nlines++], 80, "Span:     %.2f", dist);
      }

      float total_h = nlines * lh;
      float start_y = (h - total_h) * 0.5f;
      glm::vec3 header_col = {1.0f, 1.0f, 1.0f};
      glm::vec3 body_col = {0.7f, 0.7f, 0.7f};
      for (int i = 0; i < nlines; i++) {
        glm::vec3 col = (i == 0) ? header_col : body_col;
        text_draw(lines[i], 10.0f, start_y + i * lh, col, w, h);
      }
    }

    // Draw test-image.png in the bottom right corner
    if (img_tex && img_w > 0) {
      float x0 = w - img_w, y0 = h - img_h;
      float x1 = w, y1 = h;
      float verts[] = {
          x0, y0, 0.0f, 0.0f, x1, y0, 1.0f, 0.0f, x1, y1, 1.0f, 1.0f,
          x0, y0, 0.0f, 0.0f, x1, y1, 1.0f, 1.0f, x0, y1, 0.0f, 1.0f,
      };
      glm::mat4 proj = glm::ortho(0.0f, (float)w, (float)h, 0.0f);
      glUseProgram(img_prog);
      glUniformMatrix4fv(glGetUniformLocation(img_prog, "u_proj"), 1, GL_FALSE,
                         glm::value_ptr(proj));
      glUniform1i(glGetUniformLocation(img_prog, "u_tex"), 0);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, img_tex);
      glBindVertexArray(img_vao);
      glBindBuffer(GL_ARRAY_BUFFER, img_vbo);
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_DEPTH_TEST);
      glDrawArrays(GL_TRIANGLES, 0, 6);
      glEnable(GL_DEPTH_TEST);
      glDisable(GL_BLEND);
    }

    glfwSwapBuffers(win);

    if (g_screenshot_pending) {
      g_screenshot_pending = false;
      std::vector<unsigned char> pixels(w * h * 3);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
      // Flip vertically: OpenGL origin is bottom-left
      stbi_flip_vertically_on_write(1);
      char filename[64];
      time_t t = time(nullptr);
      struct tm *tm = localtime(&t);
      strftime(filename, sizeof(filename), "screenshot_%Y%m%d_%H%M%S.png", tm);
      if (stbi_write_png(filename, w, h, 3, pixels.data(), w * 3))
        printf("Screenshot saved: %s\n", filename);
      else
        fprintf(stderr, "Failed to save screenshot\n");
    }

    glfwPollEvents();
  }

  for (auto &pl : g_planets) {
    glDeleteVertexArrays(1, &pl.vao);
    glDeleteBuffers(1, &pl.vbo);
    glDeleteBuffers(1, &pl.ebo);
    glDeleteVertexArrays(1, &pl.orbit_vao);
    glDeleteBuffers(1, &pl.orbit_vbo);
  }
  for (auto &ship : g_ships) {
    glDeleteVertexArrays(1, &ship.vao);
    glDeleteBuffers(1, &ship.vbo);
  }
  glDeleteVertexArrays(1, &sun_vao);
  glDeleteBuffers(1, &sun_vbo);
  glDeleteVertexArrays(1, &axis_vao);
  glDeleteBuffers(1, &axis_vbo);
  glDeleteVertexArrays(1, &star_vao);
  glDeleteBuffers(1, &star_vbo);
  glDeleteVertexArrays(1, &mw_vao);
  glDeleteBuffers(1, &mw_vbo);
  glDeleteVertexArrays(1, &wh_vao);
  glDeleteBuffers(1, &wh_vbo);
  glDeleteVertexArrays(1, &drag_vao);
  glDeleteBuffers(1, &drag_vbo);
  glDeleteVertexArrays(1, &move_vao);
  glDeleteBuffers(1, &move_vbo);
  glDeleteProgram(prog);
  glDeleteProgram(img_prog);
  glDeleteVertexArrays(1, &img_vao);
  glDeleteBuffers(1, &img_vbo);
  if (img_tex)
    glDeleteTextures(1, &img_tex);
  glfwTerminate();
  return 0;
}
