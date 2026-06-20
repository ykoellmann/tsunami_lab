#ifndef TSUNAMI_LAB_VISUALIZATION_SHADER_H
#define TSUNAMI_LAB_VISUALIZATION_SHADER_H

#include <cstdio>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

namespace tsunami_lab {
namespace visualization {

class Shader {
public:
  void build(const char* i_vert, const char* i_frag) {
    GLuint l_vs = compile(GL_VERTEX_SHADER, i_vert);
    GLuint l_fs = compile(GL_FRAGMENT_SHADER, i_frag);

    m_id = glCreateProgram();
    glAttachShader(m_id, l_vs);
    glAttachShader(m_id, l_fs);
    glLinkProgram(m_id);

    int l_ok;
    glGetProgramiv(m_id, GL_LINK_STATUS, &l_ok);
    if (!l_ok) {
      char l_log[512];
      glGetProgramInfoLog(m_id, 512, nullptr, l_log);
      std::fprintf(stderr, "Shader link error:\n%s\n", l_log);
    }

    glDeleteShader(l_vs);
    glDeleteShader(l_fs);
  }

  void use() const { glUseProgram(m_id); }

  void setMat4(const char* i_name, const glm::mat4& i_val) const {
    glUniformMatrix4fv(glGetUniformLocation(m_id, i_name), 1, GL_FALSE,
                       &i_val[0][0]);
  }

  void setFloat(const char* i_name, float i_val) const {
    glUniform1f(glGetUniformLocation(m_id, i_name), i_val);
  }

  void setInt(const char* i_name, int i_val) const {
    glUniform1i(glGetUniformLocation(m_id, i_name), i_val);
  }

  void setVec4(const char* i_name, const glm::vec4& i_val) const {
    glUniform4fv(glGetUniformLocation(m_id, i_name), 1, &i_val[0]);
  }

  ~Shader() {
    if (m_id)
      glDeleteProgram(m_id);
  }

private:
  GLuint m_id = 0;

  GLuint compile(GLenum i_type, const char* i_src) {
    GLuint l_s = glCreateShader(i_type);
    glShaderSource(l_s, 1, &i_src, nullptr);
    glCompileShader(l_s);

    int l_ok;
    glGetShaderiv(l_s, GL_COMPILE_STATUS, &l_ok);
    if (!l_ok) {
      char l_log[512];
      glGetShaderInfoLog(l_s, 512, nullptr, l_log);
      std::fprintf(stderr, "Shader compile error:\n%s\n", l_log);
    }
    return l_s;
  }
};

} // namespace visualization
} // namespace tsunami_lab

#endif
