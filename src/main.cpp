#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

static const char *vert_src = R"(
#version 330 core
layout(location = 0) in vec3 pos;
uniform mat4 mvp;
void main() {
    gl_Position = mvp * vec4(pos, 1.0);
}
)";

static const char *frag_src = R"(
#version 330 core
out vec4 color;
void main() {
    color = vec4(1.0, 1.0, 1.0, 1.0);
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

static void mouse_button_cb(GLFWwindow *, int button, int action, int) {
  if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
    mmb_down = (action == GLFW_PRESS);
  }
}

static void cursor_pos_cb(GLFWwindow *, double mx, double my) {
  double dx = mx - last_mx;
  double dy = my - last_my;
  last_mx = mx;
  last_my = my;

  if (!mmb_down)
    return;

  const float sensitivity = 0.005f;
  cam.yaw -= (float)dx * sensitivity;
  cam.pitch += (float)dy * sensitivity;

  // Clamp pitch to avoid gimbal flip
  const float limit = glm::radians(89.0f);
  if (cam.pitch > limit)
    cam.pitch = limit;
  if (cam.pitch < -limit)
    cam.pitch = -limit;
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

  // Seed last mouse position to avoid a jump on first move
  glfwGetCursorPos(win, &last_mx, &last_my);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to init GLEW\n");
    return 1;
  }

  GLuint prog = build_program();
  GLint mvp_loc = glGetUniformLocation(prog, "mvp");

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

  glEnable(GL_DEPTH_TEST);

  while (!glfwWindowShouldClose(win)) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(win, GLFW_TRUE);

    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    glViewport(0, 0, w, h);

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 proj =
        glm::perspective(glm::radians(45.0f), (float)w / h, 0.1f, 1000.0f);
    glm::mat4 mvp = proj * cam.view();

    glUseProgram(prog);
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glfwSwapBuffers(win);
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteProgram(prog);
  glfwTerminate();
  return 0;
}
