#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <tiny_obj_loader.h>

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <cstring>

struct vec2 { float x, y; };
struct vec3 { float x, y, z; };

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 texcoords;
    bool operator==(const Vertex& other) const {
        return memcmp(this, &other, sizeof(Vertex)) == 0;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        std::size_t operator()(const Vertex& v) const {
            size_t h1 = hash<float>()(v.position.x) ^ hash<float>()(v.position.y) ^ hash<float>()(v.position.z);
            size_t h2 = hash<float>()(v.normal.x) ^ hash<float>()(v.normal.y) ^ hash<float>()(v.normal.z);
            size_t h3 = hash<float>()(v.texcoords.x) ^ hash<float>()(v.texcoords.y);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

struct Mesh {
    Vertex* vertices;
    uint32_t* indices;
    uint32_t vertexCount;
    uint32_t indexCount;
};

void framebuffer_size_callback(GLFWwindow*, int, int);
void processInput(GLFWwindow*);
Mesh loadOBJ(const std::string&);
void freeMesh(Mesh&);
GLuint loadTexture(const char* path);

void mat4_identity(float* m) {
    for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.f : 0.f;
}
void mat4_rotate_y(float* m, float angle) {
    mat4_identity(m); float c = cosf(angle), s = sinf(angle);
    m[0] = c; m[2] = s; m[8] = -s; m[10] = c;
}
void mat4_translate(float* m, float x, float y, float z) {
    mat4_identity(m); m[12] = x; m[13] = y; m[14] = z;
}
void mat4_perspective(float* m, float fovy, float aspect, float znear, float zfar) {
    mat4_identity(m);
    float f = 1.f / tanf(fovy * 0.5f);
    m[0] = f / aspect; m[5] = f;
    m[10] = (zfar + znear) / (znear - zfar);
    m[11] = -1.f; m[14] = (2.f * zfar * znear) / (znear - zfar); m[15] = 0.f;
}

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 FragColor;

uniform vec3 lightDir;
uniform sampler2D diffuseMap;

void main() {
    vec3 norm = normalize(Normal);
    vec3 light = normalize(-lightDir);
    float diff = max(dot(norm, light), 0.0);
    vec3 texColor = texture(diffuseMap, TexCoords).rgb;
    FragColor = vec4(diff * texColor, 1.0);
}
)";

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "OBJ Textured Viewer", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    Mesh mesh = loadOBJ("cube.obj");
    GLuint texID = loadTexture("textures/texture.png");

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * sizeof(Vertex), mesh.vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indexCount * sizeof(uint32_t), mesh.indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoords));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL); glCompileShader(fs);
    GLuint sp = glCreateProgram();
    glAttachShader(sp, vs); glAttachShader(sp, fs); glLinkProgram(sp);
    glDeleteShader(vs); glDeleteShader(fs);

    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        glClearColor(0.1f, 0.1f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        float time = glfwGetTime();
        float model[16], view[16], projection[16];
        mat4_rotate_y(model, time * 0.5f);
        mat4_translate(view, 0.f, 0.f, -6.f);
        mat4_perspective(projection, 3.1415926f / 4.f, 800.f / 600.f, 0.1f, 100.f);

        glUseProgram(sp);
        glUniformMatrix4fv(glGetUniformLocation(sp, "model"), 1, GL_FALSE, model);
        glUniformMatrix4fv(glGetUniformLocation(sp, "view"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(sp, "projection"), 1, GL_FALSE, projection);
        glUniform3f(glGetUniformLocation(sp, "lightDir"), 0.5f, -1.f, 0.f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        glUniform1i(glGetUniformLocation(sp, "diffuseMap"), 0);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &VBO); glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(sp);
    freeMesh(mesh);
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void freeMesh(Mesh& mesh) {
    delete[] mesh.vertices;
    delete[] mesh.indices;
    mesh.vertices = nullptr; mesh.indices = nullptr;
    mesh.vertexCount = mesh.indexCount = 0;
}

Mesh loadOBJ(const std::string& filename) {
    tinyobj::ObjReaderConfig config; config.triangulate = true;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filename, config)) throw std::runtime_error("Failed to load OBJ");
    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            for (size_t v = 0; v < 3; v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                Vertex vertex{};
                vertex.position = {
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                };
                vertex.normal = (!attrib.normals.empty() && idx.normal_index >= 0) ?
                    vec3{attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1], attrib.normals[3 * idx.normal_index + 2]} :
                    vec3{0.f, 0.f, 1.f};
                vertex.texcoords = (!attrib.texcoords.empty() && idx.texcoord_index >= 0) ?
                    vec2{attrib.texcoords[2 * idx.texcoord_index + 0], attrib.texcoords[2 * idx.texcoord_index + 1]} :
                    vec2{0.f, 0.f};

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = (uint32_t)vertices.size();
                    vertices.push_back(vertex);
                }
                indices.push_back(uniqueVertices[vertex]);
            }
            index_offset += 3;
        }
    }

    Mesh mesh;
    mesh.vertexCount = (uint32_t)vertices.size();
    mesh.indexCount = (uint32_t)indices.size();
    mesh.vertices = new Vertex[mesh.vertexCount];
    mesh.indices = new uint32_t[mesh.indexCount];
    std::copy(vertices.begin(), vertices.end(), mesh.vertices);
    std::copy(indices.begin(), indices.end(), mesh.indices);
    return mesh;
}

GLuint loadTexture(const char* path) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path, &w, &h, &channels, 0);
    if (!data) throw std::runtime_error("Failed to load texture image");

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, channels == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return tex;
}
