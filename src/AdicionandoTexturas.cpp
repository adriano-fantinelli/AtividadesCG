/* AdicionandoTexturas - Carregamento de OBJ com coords de textura e MTL.
 *
 * Controles:
 *   X - rotacionar em torno do eixo X.
 *   Y - rotacionar em torno do eixo Y.
 *   Z - rotacionar em torno do eixo Z.
 *   ESC - fechar janela.
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

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace glm;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
int  setupShader();
int  loadSimpleOBJ(const string& filePath, int& nVertices, string& textureName);
GLuint loadTexture(const string& filePath);

const GLuint WIDTH = 800, HEIGHT = 800;

const GLchar* vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texc;
layout (location = 2) in vec3 normal;

uniform mat4 model;
uniform mat4 projection;

out vec2 texCoord;
out vec3 vNormal;
out vec3 fragPos;

void main()
{
    vec4 worldPos = model * vec4(position, 1.0);
    gl_Position   = projection * worldPos;
    fragPos   = vec3(worldPos);
    texCoord  = texc;
    vNormal   = mat3(transpose(inverse(model))) * normal;
}
)";

const GLchar* fragmentShaderSource = R"(
#version 400
in vec2 texCoord;
in vec3 vNormal;
in vec3 fragPos;

uniform sampler2D texBuff;
uniform vec3 lightPos;
uniform vec3 camPos;
uniform float ka;
uniform float kd;
uniform float ks;
uniform float q;

out vec4 color;

void main()
{
    vec3 lightColor  = vec3(1.0, 1.0, 1.0);
    vec4 objectColor = texture(texBuff, texCoord);

    // Ambiente
    vec3 ambient = ka * lightColor;

    // Difuso
    vec3 N    = normalize(vNormal);
    vec3 L    = normalize(lightPos - fragPos);
    float diff  = max(dot(N, L), 0.0);
    vec3 diffuse = kd * diff * lightColor;

    // Especular
    vec3 R    = normalize(reflect(-L, N));
    vec3 V    = normalize(camPos - fragPos);
    float spec  = pow(max(dot(R, V), 0.0), q);
    vec3 specular = ks * spec * lightColor;

    vec3 result = (ambient + diffuse) * vec3(objectColor) + specular;
    color = vec4(result, objectColor.a);
}
)";

bool rotateX = false, rotateY = false, rotateZ = false;

int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "Adicionando Texturas - OBJ + MTL", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cerr << "Failed to initialize GLAD" << endl;
        return -1;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version  = glGetString(GL_VERSION);
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version: " << version << endl;

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    GLuint shaderID = setupShader();

    string textureName;
    int nVertices = 0;
    GLuint VAO = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", nVertices, textureName);
    if (VAO == (GLuint)-1)
    {
        cerr << "Erro ao carregar OBJ." << endl;
        glfwTerminate();
        return -1;
    }

    string texPath = "../assets/Modelos3D/" + textureName;
    GLuint texID = loadTexture(texPath);

    float ka = 0.2f, kd = 0.8f, ks = 0.5f, q = 32.0f;
    vec3 lightPos(2.0f, 3.0f, 2.0f);
    vec3 camPos(0.0f, 0.0f, 3.0f);

    glUseProgram(shaderID);
    glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);
    glUniform1f(glGetUniformLocation(shaderID, "ka"), ka);
    glUniform1f(glGetUniformLocation(shaderID, "kd"), kd);
    glUniform1f(glGetUniformLocation(shaderID, "ks"), ks);
    glUniform1f(glGetUniformLocation(shaderID, "q"),  q);
    glUniform3fv(glGetUniformLocation(shaderID, "lightPos"), 1, value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(shaderID, "camPos"),   1, value_ptr(camPos));

    mat4 projection = ortho(-1.0f, 1.0f, -1.0f, 1.0f, -10.0f, 10.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float angle = (float)glfwGetTime();

        mat4 model = mat4(1.0f);
        if (rotateX)
            model = rotate(model, angle, vec3(1.0f, 0.0f, 0.0f));
        else if (rotateY)
            model = rotate(model, angle, vec3(0.0f, 1.0f, 0.0f));
        else if (rotateZ)
            model = rotate(model, angle, vec3(0.0f, 0.0f, 1.0f));

        glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));

        glBindTexture(GL_TEXTURE_2D, texID);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, nVertices);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mode*/)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    { rotateX = true;  rotateY = false; rotateZ = false; }
    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    { rotateX = false; rotateY = true;  rotateZ = false; }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    { rotateX = false; rotateY = false; rotateZ = true;  }
}

int setupShader()
{
    GLint  success;
    GLchar infoLog[512];

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, nullptr);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vs, 512, nullptr, infoLog);
        cerr << "VERTEX SHADER ERROR:\n" << infoLog << endl;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fs, 512, nullptr, infoLog);
        cerr << "FRAGMENT SHADER ERROR:\n" << infoLog << endl;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(prog, 512, nullptr, infoLog);
        cerr << "SHADER PROGRAM LINK ERROR:\n" << infoLog << endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static string loadMTL(const string& mtlPath)
{
    ifstream arq(mtlPath);
    if (!arq.is_open())
    {
        cerr << "MTL nao encontrado: " << mtlPath << endl;
        return "";
    }

    string line, texName;
    while (getline(arq, line))
    {
        istringstream ss(line);
        string token;
        ss >> token;
        if (token == "map_Kd")
        {
            ss >> texName;
            break;
        }
    }
    return texName;
}

int loadSimpleOBJ(const string& filePath, int& nVertices, string& textureName)
{
    vector<vec3>    positions;
    vector<vec2>    texCoords;
    vector<vec3>    normals;
    vector<GLfloat> vBuffer;

    // Diretório base do OBJ (para localizar o MTL na mesma pasta);
    string baseDir;
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != string::npos)
        baseDir = filePath.substr(0, lastSlash + 1);

    ifstream arq(filePath);
    if (!arq.is_open())
    {
        cerr << "Erro ao abrir: " << filePath << endl;
        return -1;
    }

    string line;
    while (getline(arq, line))
    {
        istringstream ssline(line);
        string token;
        ssline >> token;

        if (token == "mtllib")
        {
            // Ler o MTL referenciado e extrair o nome da textura.
            string mtlFile;
            ssline >> mtlFile;
            textureName = loadMTL(baseDir + mtlFile);
        }
        else if (token == "v")
        {
            vec3 pos;
            ssline >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (token == "vt")
        {
            vec2 tc;
            ssline >> tc.s >> tc.t;
            texCoords.push_back(tc);
        }
        else if (token == "vn")
        {
            vec3 n;
            ssline >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        else if (token == "f")
        {
            string faceToken;
            while (ssline >> faceToken)
            {
                int vi = 0, ti = 0, ni = 0;
                istringstream ss(faceToken);
                string idx;

                if (getline(ss, idx, '/')) vi = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(ss, idx, '/')) ti = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(ss, idx))      ni = !idx.empty() ? stoi(idx) - 1 : 0;

                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);

                if (!texCoords.empty())
                {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                }
                else
                {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                }

                if (!normals.empty())
                {
                    vBuffer.push_back(normals[ni].x);
                    vBuffer.push_back(normals[ni].y);
                    vBuffer.push_back(normals[ni].z);
                }
                else
                {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(1.0f);
                }
            }
        }
    }
    arq.close();

    cout << "OBJ carregado. Gerando VAO..." << endl;

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vBuffer.size() * sizeof(GLfloat),
                 vBuffer.data(),
                 GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    const GLsizei stride = 8 * sizeof(GLfloat);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 8); // 8 floats por vértice
    cout << "Vertices: " << nVertices << " | Textura MTL: " << textureName << endl;

    return VAO;
}

GLuint loadTexture(const string& filePath)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &nChannels, 0);

    if (data)
    {
        GLenum fmt = (nChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        cout << "Textura carregada: " << filePath
             << " (" << width << "x" << height << ", " << nChannels << "ch)" << endl;
    }
    else
    {
        cerr << "Falha ao carregar textura: " << filePath << endl;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}
