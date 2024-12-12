#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

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

static GLuint build_program() {
  GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);

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

  GLuint prog = build_program();
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

    // Draw orbit target indicator
    glm::mat4 axis_mvp = mvp * glm::translate(glm::mat4(1.0f), cam.target);
    glUniform1i(use_vc_loc, 1);
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(axis_mvp));
    glBindVertexArray(axis_vao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 6);

    glfwSwapBuffers(win);
    glfwPollEvents();
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
