#if defined(ANDROID)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
typedef char GLchar;
#else
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

#include <set>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "base/logging.h"
#include "file/vfs.h"
#include "gfx_es2/glsl_program.h"

static std::set<GLSLProgram *> active_programs;

bool CompileShader(const char *source, GLuint shader, const char *filename) {
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
#define MAX_INFO_LOG_SIZE 2048
		GLchar infoLog[MAX_INFO_LOG_SIZE];
    GLsizei len;
		glGetShaderInfoLog(shader, MAX_INFO_LOG_SIZE, &len, infoLog);
    infoLog[len] = '\0';
		ELOG("Error in shader compilation of %s!\n", filename);
		ELOG("Info log: %s\n", infoLog);
		ELOG("Shader source:\n%s\n", (const char *)source);
    exit(1);
		return false;
	}
	return true;
}

GLSLProgram *glsl_create(const char *vshader, const char *fshader) {
  GLSLProgram *program = new GLSLProgram();
  program->program_ = 0;
  program->vsh_ = 0;
  program->fsh_ = 0;
	strcpy(program->name, vshader + strlen(vshader) - 16);
  strcpy(program->vshader_filename, vshader);
  strcpy(program->fshader_filename, fshader);
  if (glsl_recompile(program)) {
    active_programs.insert(program);
  }
  register_gl_resource_holder(program);
  return program;
}

bool glsl_up_to_date(GLSLProgram *program) {
  struct stat vs, fs;
  stat(program->vshader_filename, &vs);
  stat(program->fshader_filename, &fs);
  if (vs.st_mtime != program->vshader_mtime ||
      fs.st_mtime != program->fshader_mtime) {
    return false;
  } else {
    return true;
  }
}

void glsl_refresh() {
  ILOG("glsl_refresh()");
  for (std::set<GLSLProgram *>::const_iterator iter = active_programs.begin();
       iter != active_programs.end(); ++iter) {
    if (!glsl_up_to_date(*iter)) {
      glsl_recompile(*iter);
    }
  }
}

bool glsl_recompile(GLSLProgram *program) {
  struct stat vs, fs;
  stat(program->vshader_filename, &vs);
  stat(program->fshader_filename, &fs);
  program->vshader_mtime = vs.st_mtime;
  program->fshader_mtime = fs.st_mtime;

  size_t sz;
	char *vsh_src = (char *)VFSReadFile(program->vshader_filename, &sz);
  if (!vsh_src) {
    ELOG("File missing: %s", vsh_src);
    return false;
  }
	char *fsh_src = (char *)VFSReadFile(program->fshader_filename, &sz);
  if (!fsh_src) {
    ELOG("File missing: %s", fsh_src);
    delete [] vsh_src;
    return false;
  }

	GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
	const GLchar *vsh_str = (const GLchar *)(vsh_src);
	if (!CompileShader(vsh_str, vsh, program->vshader_filename)) {
    return false;
  }
  delete [] vsh_src;

	const GLchar *fsh_str = (const GLchar *)(fsh_src);
	GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);
  if (!CompileShader(fsh_str, fsh, program->fshader_filename)) {
    glDeleteShader(vsh);
    return false;
  }
	delete [] fsh_src;

  GLuint prog = glCreateProgram();
	glAttachShader(prog, vsh);
	glAttachShader(prog, fsh);

	glLinkProgram(prog);

	GLint linkStatus;
	glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE) {
		GLint bufLength = 0;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &bufLength);
		if (bufLength) {
			char* buf = new char[bufLength];
	  	glGetProgramInfoLog(prog, bufLength, NULL, buf);
			FLOG("Could not link program:\n %s", buf);
			delete [] buf;  // we're dead!
		}
    glDeleteShader(vsh);
    glDeleteShader(fsh);
		return false;
	}

  // Destroy the old program, if any.
  if (program->program_) {
    glDeleteProgram(program->program_);
  }

  program->program_ = prog;
  program->vsh_ = vsh;
  program->fsh_ = vsh;

	program->sampler0 = glGetUniformLocation(program->program_, "sampler0");
	program->sampler1 = glGetUniformLocation(program->program_, "sampler1");

	program->a_position  = glGetAttribLocation(program->program_, "a_position");
	program->a_color     = glGetAttribLocation(program->program_, "a_color");
	program->a_normal    = glGetAttribLocation(program->program_, "a_normal");
	program->a_texcoord0 = glGetAttribLocation(program->program_, "a_texcoord0");
	program->a_texcoord1 = glGetAttribLocation(program->program_, "a_texcoord1");

	program->u_worldviewproj = glGetUniformLocation(program->program_, "u_worldviewproj");
	program->u_world = glGetUniformLocation(program->program_, "u_world");
	program->u_viewproj = glGetUniformLocation(program->program_, "u_viewproj");
	program->u_fog = glGetUniformLocation(program->program_, "u_fog");
	program->u_sundir = glGetUniformLocation(program->program_, "u_sundir");
	program->u_camerapos = glGetUniformLocation(program->program_, "u_camerapos");

	//ILOG("Shader compilation success: %s %s",
  //     program->vshader_filename,
  //     program->fshader_filename);
  return true;
}

void GLSLProgram::GLLost() {
  ILOG("Restoring GLSL program %s/%s", this->vshader_filename, this->fshader_filename);
  this->program_ = 0;
  this->vsh_ = 0;
  this->fsh_ = 0;
  glsl_recompile(this);  
}


int glsl_attrib_loc(const GLSLProgram *program, const char *name) {
  return glGetAttribLocation(program->program_, name);
}

int glsl_uniform_loc(const GLSLProgram *program, const char *name) {
  return glGetUniformLocation(program->program_, name);
}

void glsl_destroy(GLSLProgram *program) {
  unregister_gl_resource_holder(program);
	glDeleteShader(program->vsh_);
	glDeleteShader(program->fsh_);
	glDeleteProgram(program->program_);
  active_programs.erase(program);
  delete program;
}

void glsl_bind(const GLSLProgram *program) {
	glUseProgram(program->program_);
}

void glsl_unbind() {
	glUseProgram(0);
}
