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
uniform vec3 u_color;
uniform bool u_use_vertex_color;
out vec4 color;
void main() {
    color = vec4(u_use_vertex_color ? frag_color : u_color, 1.0);
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
static bool mmb_down = false;
static double last_mx = 0.0;
static double last_my = 0.0;

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
    if (cam.pitch > limit)
      cam.pitch = limit;
    if (cam.pitch < -limit)
      cam.pitch = -limit;
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

  // Triangle
  float verts[] = {
      0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f,
  };

  GLuint vao, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

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

  while (!glfwWindowShouldClose(win)) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(win, GLFW_TRUE);

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

    // Draw triangle
    glUniform1i(use_vc_loc, 0);
    glUniform3f(color_loc, 1.0f, 1.0f, 1.0f);
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

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

  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteVertexArrays(1, &axis_vao);
  glDeleteBuffers(1, &axis_vbo);
  glDeleteVertexArrays(1, &star_vao);
  glDeleteBuffers(1, &star_vbo);
  glDeleteProgram(prog);
  glfwTerminate();
  return 0;
}
