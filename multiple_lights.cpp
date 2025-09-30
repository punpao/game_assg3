#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

// === utility: load/compile/link shaders (single-file, no external Shader class) ===
static std::string readTextFile(const std::string& path) {
    std::ifstream ifs(path);
    std::stringstream ss; ss << ifs.rdbuf();
    return ss.str();
}
static GLuint compile(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0'); glGetShaderInfoLog(s, len, nullptr, log.data());
        std::cerr << "Shader compile error:\n" << log << std::endl;
    }
    return s;
}
static GLuint link(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0'); glGetProgramInfoLog(p, len, nullptr, log.data());
        std::cerr << "Program link error:\n" << log << std::endl;
    }
    glDetachShader(p, vs); glDetachShader(p, fs);
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// === camera minimal (orbit) ===
static float g_time = 0.f;
glm::mat4 makeView() {
    float radius = 6.5f;
    float camX = sin(g_time * 0.3f) * radius;
    float camZ = cos(g_time * 0.3f) * radius;
    glm::vec3 pos(camX, 3.0f, camZ);
    return glm::lookAt(pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
}

// === mesh: parametric "revolve + wave" kinetic sculpture ===
// base curve (superellipse-ish) in XZ, then revolve along Y; animate radius over time
struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
};
Mesh makeSculpture(int rowRings = 140, int colSegments = 180) {
    std::vector<float> v; v.reserve(rowRings * colSegments * 8);
    std::vector<unsigned int> idx; idx.reserve((rowRings - 1) * colSegments * 6);

    auto toIndex = [colSegments](int r, int c) {
        int C = (c + colSegments) % colSegments;
        return r * colSegments + C;
        };

    // generate vertices (pos, normal, tex)
    for (int r = 0; r < rowRings; ++r) {
        float vParam = (float)r / (rowRings - 1);        // 0..1 along Y
        float y = (vParam - 0.5f) * 3.0f;              // height
        for (int c = 0; c < colSegments; ++c) {
            float uParam = (float)c / colSegments;     // 0..1 around
            float theta = uParam * glm::two_pi<float>();

            // time-varying radius: base superellipse + travelling wave
            float a = 1.0f, b = 0.5f;                    // superellipse radii
            float n = 2.5f;                             // superellipse exponent
            // superellipse in 2D (r0 around y-axis)
            float cx = powf(fabs(cos(theta)), 2 / n) * a * (cos(theta) >= 0 ? 1 : -1);
            float cz = powf(fabs(sin(theta)), 2 / n) * b * (sin(theta) >= 0 ? 1 : -1);
            float r0 = sqrtf(cx * cx + cz * cz);

            float wave = 0.25f * sin(6.0f * uParam * glm::two_pi<float>() - 4.0f * vParam * glm::two_pi<float>() + g_time * 1.5f);
            float radius = r0 * (1.0f + wave);

            float x = radius * cos(theta);
            float z = radius * sin(theta);

            // approximate normal via partial derivatives in parametric form
            // For visual quality we can normalize (x,z) in the horizontal plane:
            glm::vec3 tTheta(-radius * sin(theta), 0.0f, radius * cos(theta));
            glm::vec3 tY(0.0f, 1.0f, 0.0f);
            glm::vec3 nrm = glm::normalize(glm::cross(tTheta, tY)); // outward approx

            v.insert(v.end(), { x, y, z,  nrm.x, nrm.y, nrm.z,  uParam, vParam });
        }
    }
    for (int r = 0; r < rowRings - 1; ++r) {
        for (int c = 0; c < colSegments; ++c) {
            unsigned int i0 = toIndex(r, c);
            unsigned int i1 = toIndex(r, c + 1);
            unsigned int i2 = toIndex(r + 1, c);
            unsigned int i3 = toIndex(r + 1, c + 1);
            idx.insert(idx.end(), { i0,i2,i1,  i1,i2,i3 });
        }
    }

    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);

    GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    m.indexCount = (GLsizei)idx.size();
    return m;
}

struct LightCube {
    GLuint vao = 0, vbo = 0;
    GLsizei count = 0;
};
LightCube makeLightCube() {
    float s = 0.08f;
    float verts[] = {
        -s,-s,-s,  s,-s,-s,  s, s,-s,  -s, s,-s,
        -s,-s, s,  s,-s, s,  s, s, s,  -s, s, s
    };
    unsigned int idx[] = {
        0,1,2, 2,3,0, 1,5,6, 6,2,1, 5,4,7, 7,6,5,
        4,0,3, 3,7,4, 3,2,6, 6,7,3, 4,5,1, 1,0,4
    };
    GLuint ebo;
    LightCube c; c.count = sizeof(idx) / sizeof(unsigned int);
    glGenVertexArrays(1, &c.vao);
    glGenBuffers(1, &c.vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(c.vao);
    glBindBuffer(GL_ARRAY_BUFFER, c.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    return c;
}

// positions of 4 point lights
static glm::vec3 pointLights[4] = {
    { 1.5f,  1.2f,  1.5f},
    {-1.8f,  1.0f,  1.2f},
    { 1.6f,  1.5f, -1.6f},
    {-1.4f,  1.4f, -1.3f}
};

int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Kinetic Sculpture - Multiple Lights", nullptr, nullptr);
    if (!win) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD load failed\n"; return -1; }

    glEnable(GL_DEPTH_TEST);

    // load shaders
    GLuint prog = link(
        compile(GL_VERTEX_SHADER, readTextFile("sculpture.vs")),
        compile(GL_FRAGMENT_SHADER, readTextFile("sculpture.fs"))
    );
    GLuint progLight = link(
        compile(GL_VERTEX_SHADER, readTextFile("light_cube.vs")),
        compile(GL_FRAGMENT_SHADER, readTextFile("light_cube.fs"))
    );

    Mesh sculpture = makeSculpture();
    LightCube cube = makeLightCube();

    // material constants
    glm::vec3 matAmbient(0.15f);
    glm::vec3 matDiffuse(0.7f, 0.75f, 0.8f);
    glm::vec3 matSpecular(0.9f);
    float shininess = 48.0f;

    while (!glfwWindowShouldClose(win)) {
        g_time = (float)glfwGetTime();
        glfwPollEvents();

        int w, h; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.02f, 0.02f, 0.035f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), w > 0 ? (float)w / h : 1.0f, 0.1f, 100.0f);
        glm::mat4 view = makeView();

        // === animate lights gently ===
        for (int i = 0; i < 4; ++i) {
            float phase = i * glm::half_pi<float>();
            pointLights[i].x = 1.8f * sin(g_time * 0.7f + phase);
            pointLights[i].z = 1.8f * cos(g_time * 0.7f + phase);
            pointLights[i].y = 1.0f + 0.4f * sin(g_time * 1.3f + i);
        }

        // === draw sculpture ===
        glUseProgram(prog);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE, glm::value_ptr(view));

        // world transform (slow spin)
        glm::mat4 model(1.0f);
        model = glm::rotate(model, g_time * 0.25f, glm::vec3(0, 1, 0));
        glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, glm::value_ptr(model));

        // material
        glUniform3fv(glGetUniformLocation(prog, "material.ambient"), 1, glm::value_ptr(matAmbient));
        glUniform3fv(glGetUniformLocation(prog, "material.diffuse"), 1, glm::value_ptr(matDiffuse));
        glUniform3fv(glGetUniformLocation(prog, "material.specular"), 1, glm::value_ptr(matSpecular));
        glUniform1f(glGetUniformLocation(prog, "material.shininess"), shininess);

        // camera position for specular
        glm::vec3 camPos = glm::vec3(glm::inverse(view)[3]);
        glUniform3fv(glGetUniformLocation(prog, "uViewPos"), 1, glm::value_ptr(camPos));

        // directional light
        glUniform3f(glGetUniformLocation(prog, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(prog, "dirLight.ambient"), 0.04f, 0.04f, 0.05f);
        glUniform3f(glGetUniformLocation(prog, "dirLight.diffuse"), 0.25f, 0.25f, 0.3f);
        glUniform3f(glGetUniformLocation(prog, "dirLight.specular"), 0.3f, 0.3f, 0.35f);

        // 4 point lights
        for (int i = 0; i < 4; ++i) {
            std::string base = "pointLights[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(prog, (base + ".position").c_str()), 1, glm::value_ptr(pointLights[i]));
            glUniform3f(glGetUniformLocation(prog, (base + ".ambient").c_str()), 0.02f, 0.02f, 0.02f);
            glUniform3f(glGetUniformLocation(prog, (base + ".diffuse").c_str()), 0.9f, 0.9f, 0.9f);
            glUniform3f(glGetUniformLocation(prog, (base + ".specular").c_str()), 1.0f, 1.0f, 1.0f);
            glUniform1f(glGetUniformLocation(prog, (base + ".constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(prog, (base + ".linear").c_str()), 0.14f);
            glUniform1f(glGetUniformLocation(prog, (base + ".quadratic").c_str()), 0.07f);
        }

        glBindVertexArray(sculpture.vao);
        glDrawElements(GL_TRIANGLES, sculpture.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // === draw light cubes ===
        glUseProgram(progLight);
        glUniformMatrix4fv(glGetUniformLocation(progLight, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(progLight, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glBindVertexArray(cube.vao);
        for (int i = 0; i < 4; ++i) {
            glm::mat4 m(1.0f); m = glm::translate(m, pointLights[i]);
            glUniformMatrix4fv(glGetUniformLocation(progLight, "uModel"), 1, GL_FALSE, glm::value_ptr(m));
            glDrawElements(GL_TRIANGLES, cube.count, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);

        glfwSwapBuffers(win);
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, 1);
    }

    glfwDestroyWindow(win); glfwTerminate();
    return 0;
}
