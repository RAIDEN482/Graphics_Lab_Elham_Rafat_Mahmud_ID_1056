#include "glad.h"
#include "glfw3.h"

#include <iostream>

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window, bool &redMode, bool &whiteMode);

const char *vertexShaderSource ="#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos, 1.0);\n"
    "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec4 ourColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = ourColor;\n"
    "}\n\0";

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

   
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "0432320005101056", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    // build shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // triangle vertices
    float vertices[] = {
         0.5f, -0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // animation variables
    float t = 0.0f;
    bool redMode = false;
    bool whiteMode = false;

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        processInput(window, redMode, whiteMode);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // color logic
        float r, g, b;
        if (redMode) {
            r = 1.0f; g = 0.0f; b = 0.0f; // permanent red
        } else if (whiteMode) {
            r = g = b = 1.0f; // white while W held
        } else {
            // animate cyan (0,1,1) → magenta (1,0,1)
            t += 0.01f;
            if (t > 1.0f) t = 0.0f;
            r = t;        // goes 0 → 1
            g = 1.0f - t; // goes 1 → 0
            b = 1.0f;     // stays 1
        }

        int vertexColorLocation = glGetUniformLocation(shaderProgram, "ourColor");
        glUniform4f(vertexColorLocation, r, g, b, 1.0f);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window, bool &redMode, bool &whiteMode)
{
    // Close window on E
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        std::cout << "E pressed, closing window..." << std::endl;
        glfwSetWindowShouldClose(window, true);
    }

    // Press R to make the triangle permanently red
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        redMode = true;
        whiteMode = false;
    }

    // White while W is held
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        whiteMode = true;
    } else {
        whiteMode = false;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
