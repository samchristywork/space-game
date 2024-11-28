#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>

static const char *vert_src = R"(
#version 330 core
layout(location = 0) in vec2 pos;
void main() {
    gl_Position = vec4(pos, 0.0, 1.0);
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

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to init GLEW\n");
    return 1;
  }

  GLuint prog = build_program();

  float verts[] = {
      0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
  };

  GLuint vao, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

  while (!glfwWindowShouldClose(win)) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(win, GLFW_TRUE);

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
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
