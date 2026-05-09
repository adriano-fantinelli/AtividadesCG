/* VivencialObjetos3D - Selecionando e aplicando transformações em objetos 3D.

 * Controles:
 *   Tab       - Seleciona o próximo objeto e o selecionado fica brilhante.
 *   R         - Modo Rotação.
 *   T         - Modo Translação.
 *   S         - Modo Escala.
 *
 *   Modo Rotação:
 *     X       - Alterna rotação contínua no eixo X.
 *     Y       - Alterna rotação contínua no eixo Y.
 *     Z       - Alterna rotação contínua no eixo Z.
 *
 *   Modo Translação:
 *     Seta ↑↓ - Translada no eixo Y.
 *     Seta ←→ - Translada no eixo X.
 *     I / J   - Translada no eixo Z.
 *
 *   Modo Escala:
 *     +       - Aumenta escala uniforme.
 *     -       - Diminui escala uniforme.
 *
 *   Esc       - Fecha a janela.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Estrutura por objeto.
struct Object3D
{
    GLuint      VAO;
    int         nVertices;
    glm::vec3   position;
    glm::vec3   angles;
    glm::vec3   scale;
    glm::vec3   color;
    bool        rotX = false;
    bool        rotY = false;
    bool        rotZ = false;
};

enum Mode { ROTATE, TRANSLATE, SCALE };

const GLuint WIDTH = 1280, HEIGHT = 720;

vector<Object3D> objects;
int   selectedObj  = 0;
Mode  currentMode  = ROTATE;

const float TRANSLATE_SPEED = 2.0f;
const float SCALE_SPEED     = 1.0f;
const float ROTATE_SPEED    = 2.0f;

const GLchar* vertexShaderSource = R"(
#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 colorTint;
uniform float highlight;

out vec4 finalColor;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    finalColor  = vec4(color * colorTint * highlight, 1.0);
}
)";

const GLchar* fragmentShaderSource = R"(
#version 450
in  vec4 finalColor;
out vec4 fragColor;
void main()
{
    fragColor = finalColor;
}
)";

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
int  setupShader();
int  loadSimpleOBJ(const string& filePATH, int& nVertices, glm::vec3 color);
void processInput(GLFWwindow* window, float deltaTime);

int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "VivencialObjetos3D -- Adriano Fantinelli", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version  = glGetString(GL_VERSION);
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version: " << version << endl;

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    GLuint shaderID = setupShader();
    glUseProgram(shaderID);

    // Carrega modelos.
    struct ModelDesc { string path; glm::vec3 color; glm::vec3 position; };
    vector<ModelDesc> descs = {
        { "../assets/Modelos3D/Cube.obj",          glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(-1.8f, 0.0f, 0.0f) },
        { "../assets/Modelos3D/Suzanne.obj",        glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3( 0.0f, 0.0f, 0.0f) },
        { "../assets/Modelos3D/SuzanneSubdiv1.obj", glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3( 1.8f, 0.0f, 0.0f) },
    };

    for (auto& d : descs)
    {
        Object3D obj;
        obj.VAO        = loadSimpleOBJ(d.path, obj.nVertices, d.color);
        obj.position   = d.position;
        obj.angles     = glm::vec3(0.0f);
        obj.scale      = glm::vec3(0.5f);
        obj.color      = d.color;
        if (obj.VAO != (GLuint)-1)
            objects.push_back(obj);
    }

    if (objects.empty())
    {
        cerr << "Nenhum modelo carregado. Verifique os caminhos dos .OBJ." << endl;
        glfwTerminate();
        return -1;
    }

    // Uniforms de câmera.
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        (float)WIDTH / (float)HEIGHT,
        0.1f, 100.0f
    );

    GLint modelLoc      = glGetUniformLocation(shaderID, "model");
    GLint viewLoc       = glGetUniformLocation(shaderID, "view");
    GLint projLoc       = glGetUniformLocation(shaderID, "projection");
    GLint colorTintLoc  = glGetUniformLocation(shaderID, "colorTint");
    GLint highlightLoc  = glGetUniformLocation(shaderID, "highlight");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glEnable(GL_DEPTH_TEST);

    float lastTime = (float)glfwGetTime();

    // Loop principal.
    while (!glfwWindowShouldClose(window))
    {
        float currentTime = (float)glfwGetTime();
        float deltaTime   = currentTime - lastTime;
        lastTime          = currentTime;

        glfwPollEvents();
        processInput(window, deltaTime);

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Acumula rotações e desenha cada objeto.
        for (int i = 0; i < (int)objects.size(); i++)
        {
            Object3D& obj = objects[i];

            // Acumula ângulos de rotação.
            if (obj.rotX) obj.angles.x += ROTATE_SPEED * deltaTime;
            if (obj.rotY) obj.angles.y += ROTATE_SPEED * deltaTime;
            if (obj.rotZ) obj.angles.z += ROTATE_SPEED * deltaTime;

            // Monta matriz model.
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, obj.position);
            model = glm::rotate(model, obj.angles.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, obj.angles.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, obj.angles.z, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, obj.scale);

            // Objeto selecionado aparece brilhante.
            float highlight = (i == selectedObj) ? 1.6f : 0.75f;

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3fv(colorTintLoc, 1, glm::value_ptr(obj.color));
            glUniform1f(highlightLoc, highlight);

            glBindVertexArray(obj.VAO);
            glDrawArrays(GL_TRIANGLES, 0, obj.nVertices);
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    for (auto& obj : objects)
        glDeleteVertexArrays(1, &obj.VAO);

    glfwTerminate();
    return 0;
}

// Polling contínuo para translação e escala.
void processInput(GLFWwindow* window, float deltaTime)
{
    if (objects.empty()) return;
    Object3D& obj = objects[selectedObj];

    if (currentMode == TRANSLATE)
    {
        float spd = TRANSLATE_SPEED * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) obj.position.y += spd;
        if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) obj.position.y -= spd;
        if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) obj.position.x -= spd;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) obj.position.x += spd;
        if (glfwGetKey(window, GLFW_KEY_I)     == GLFW_PRESS) obj.position.z -= spd;
        if (glfwGetKey(window, GLFW_KEY_J)     == GLFW_PRESS) obj.position.z += spd;
    }
    else if (currentMode == SCALE)
    {
        float spd = SCALE_SPEED * deltaTime;

        bool scaleUp = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS
                    || glfwGetKey(window, GLFW_KEY_EQUAL)         == GLFW_PRESS
                    || glfwGetKey(window, GLFW_KEY_KP_ADD)        == GLFW_PRESS;

        bool scaleDown = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS
                      || glfwGetKey(window, GLFW_KEY_MINUS)        == GLFW_PRESS
                      || glfwGetKey(window, GLFW_KEY_KP_SUBTRACT)  == GLFW_PRESS;

        if (scaleUp)
        {
            obj.scale += glm::vec3(spd);
            if (obj.scale.x > 5.0f) obj.scale = glm::vec3(5.0f);
        }
        if (scaleDown)
        {
            obj.scale -= glm::vec3(spd);
            if (obj.scale.x < 0.1f) obj.scale = glm::vec3(0.1f);
        }
    }
}

// Eventos discretos.
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (action != GLFW_PRESS) return;

    // Seleção de objeto.
    if (key == GLFW_KEY_TAB)
    {
        selectedObj = (selectedObj + 1) % (int)objects.size();
        cout << "Objeto selecionado: " << selectedObj << endl;
    }

    // Troca de modo.
    if (key == GLFW_KEY_R) { currentMode = ROTATE;    cout << "Modo: ROTACAO"    << endl; }
    if (key == GLFW_KEY_T) { currentMode = TRANSLATE;  cout << "Modo: TRANSLACAO" << endl; }
    if (key == GLFW_KEY_S) { currentMode = SCALE;      cout << "Modo: ESCALA"     << endl; }

    // Eixos de rotação.
    if (currentMode == ROTATE && !objects.empty())
    {
        Object3D& obj = objects[selectedObj];
        if (key == GLFW_KEY_X) { obj.rotX = !obj.rotX; cout << "rotX: " << obj.rotX << endl; }
        if (key == GLFW_KEY_Y) { obj.rotY = !obj.rotY; cout << "rotY: " << obj.rotY << endl; }
        if (key == GLFW_KEY_Z) { obj.rotZ = !obj.rotZ; cout << "rotZ: " << obj.rotZ << endl; }
    }
}

int setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    GLint  success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "VERTEX SHADER ERROR:\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "FRAGMENT SHADER ERROR:\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cout << "SHADER LINK ERROR:\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

// Carrega arquivo .OBJ e retorna VAO.
int loadSimpleOBJ(const string& filePATH, int& nVertices, glm::vec3 color)
{
    vector<glm::vec3> vertices;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat>   vBuffer;

    ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open())
    {
        cerr << "Erro ao abrir: " << filePATH << endl;
        return -1;
    }

    string line;
    while (getline(arqEntrada, line))
    {
        istringstream ssline(line);
        string word;
        ssline >> word;

        if (word == "v")
        {
            glm::vec3 v;
            ssline >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        }
        else if (word == "vt")
        {
            glm::vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            glm::vec3 vn;
            ssline >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if (word == "f")
        {
            while (ssline >> word)
            {
                int vi = 0, ti = 0, ni = 0;
                istringstream ss(word);
                string index;

                if (getline(ss, index, '/')) vi = !index.empty() ? stoi(index) - 1 : 0;
                if (getline(ss, index, '/')) ti = !index.empty() ? stoi(index) - 1 : 0;
                if (getline(ss, index))      ni = !index.empty() ? stoi(index) - 1 : 0;

                vBuffer.push_back(vertices[vi].x);
                vBuffer.push_back(vertices[vi].y);
                vBuffer.push_back(vertices[vi].z);
                vBuffer.push_back(color.r);
                vBuffer.push_back(color.g);
                vBuffer.push_back(color.b);
            }
        }
    }
    arqEntrada.close();

    cout << "Carregado: " << filePATH
         << "  (" << vBuffer.size() / 6 << " vértices)" << endl;

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat),
                 vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                          (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 6);
    return VAO;
}
