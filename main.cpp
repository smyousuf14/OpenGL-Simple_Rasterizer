#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <sstream>

// Shader sources
const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 mvp;
void main() {
    gl_Position = mvp * vec4(aPos, 1.0);
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main() {
    FragColor = vec4(color, 1.0);
}
)glsl";

const char* outlineFragmentShader = R"glsl(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
)glsl";

struct Mesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<unsigned int> edgeIndices;
    glm::vec3 color;
};

Mesh loadOBJ(const char* path) {
    Mesh mesh;
    std::ifstream file(path);
    std::string line;
    std::set<std::pair<unsigned int, unsigned int>> edges;

    while (std::getline(file, line)) {
        if (line.substr(0, 2) == "v ") {
            std::istringstream ss(line.substr(2));
            float x, y, z;
            ss >> x >> y >> z;
            mesh.vertices.insert(mesh.vertices.end(), { x, y, z });
        }
        else if (line.substr(0, 2) == "f ") {
            std::istringstream ss(line.substr(2));
            std::string token;
            std::vector<unsigned int> face;

            while (ss >> token) {
                // Handle format like "f v1/vt1/vn1 v2/vt2/vn2 ..."
                size_t pos = token.find('/');
                if (pos != std::string::npos) {
                    token = token.substr(0, pos);
                }
                face.push_back(std::stoul(token) - 1); // Convert to 0-based
            }

            // Add triangle indices
            if (face.size() >= 3) {
                mesh.indices.insert(mesh.indices.end(), { face[0], face[1], face[2] });
            }

            // Create edges
            for (size_t i = 0; i < face.size(); i++) {
                unsigned int a = face[i];
                unsigned int b = face[(i + 1) % face.size()];
                if (a > b) std::swap(a, b);
                edges.insert({ a, b });
            }
        }
    }

    // Convert edges to index array
    for (auto& edge : edges) {
        mesh.edgeIndices.push_back(edge.first);
        mesh.edgeIndices.push_back(edge.second);
    }

    mesh.color = glm::vec3(0.0f, 0.0f, 1.0f);
    return mesh;
}

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        std::cerr << "Shader error:\n" << infoLog << std::endl;
    }
    return id;
}

unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking error:\n" << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Blue Prism Outline", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Load mesh data
    Mesh mesh = loadOBJ("prism.obj");

    // Create and bind buffers
    unsigned int VAO, VBO, EBO, edgeEBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenBuffers(1, &edgeEBO);

    glBindVertexArray(VAO);

    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float), mesh.vertices.data(), GL_STATIC_DRAW);

    // Main element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);

    // Edge element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.edgeIndices.size() * sizeof(unsigned int), mesh.edgeIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Create shaders
    unsigned int mainShader = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    unsigned int outlineShader = createShaderProgram(vertexShaderSource, outlineFragmentShader);

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Set up transformation matrices
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(
            glm::vec3(3, 2, 3),
            glm::vec3(0, 0, 0),
            glm::vec3(0, 1, 0)
        );
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 mvp = projection * view * model;

        // Draw main prism
        glUseProgram(mainShader);
        glUniformMatrix4fv(glGetUniformLocation(mainShader, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3fv(glGetUniformLocation(mainShader, "color"), 1, &mesh.color[0]);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);

        // Draw outline
        glUseProgram(outlineShader);
        glUniformMatrix4fv(glGetUniformLocation(outlineShader, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeEBO);
        glLineWidth(3.0f);
        glDrawElements(GL_LINES, mesh.edgeIndices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteBuffers(1, &edgeEBO);
    glDeleteProgram(mainShader);
    glDeleteProgram(outlineShader);

    glfwTerminate();
    return 0;
}