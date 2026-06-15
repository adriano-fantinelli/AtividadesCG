/* ============================================================
 * GrauB.cpp  — Pipeline Gráfico Completo
 * Autor: Adriano Fantinelli
 * ============================================================
 *
 * Integra todos os componentes do semestre em uma única cena:
 *   · Múltiplos OBJ/MTL com textura UV.
 *   · Iluminação Phong com 3 fontes pontuais (key / fill / back + rim).
 *   · Câmera FPS (WASD + mouse).
 *   · Seleção e transformação de objetos (R/T/S).
 *   · Animação por curvas de Bézier cúbico.
 *   · Cena definida em assets/scene.txt.
 *
 * CONTROLES  ──────────────────────────────────────────────────
 *  M           — alternar entre modo CÂMERA e OBJETO.
 *  [Câmera]    W/A/S/D + Mouse — mover / rotacionar.
 *  [Objeto]    Tab — selecionar próximo.
 *              R / T / S — modo Rotação / Translação / Escala.
 *              Rotação: X Y Z  — toggle eixo.
 *              Translação: Setas e I/K.
 *              Escala: = aumenta e - diminui.
 *  1 / 2 / 3  — toggle key / fill / back light.
 *  4           — toggle textura (demonstra Ka/Kd/Ks sem mapa).
 *  Space       — pausar / retomar animações Bézier.
 *  ESC         — fechar.
 * ============================================================ */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace glm;

// Constantes
const GLuint WIN_W    = 1280, WIN_H = 720;
const float  TRANS_V  = 3.0f;   // unidades / segundo
const float  SCALE_V  = 1.5f;
const float  ROT_V    = 90.0f;  // graus / segundo

// Material — coeficientes Phong lidos do .MTL.
struct Material {
    vec3   Ka{0.2f, 0.2f, 0.2f};   // [MTL Ka] reflexão ambiente
    vec3   Kd{1.0f, 1.0f, 1.0f};   // [MTL Kd] reflexão difusa
    vec3   Ks{0.5f, 0.5f, 0.5f};   // [MTL Ks] reflexão especular
    float  Ns{32.0f};              // [MTL Ns] expoente de brilho
    string textureName;            // [MTL map_Kd] arquivo de textura
};

// Fonte de luz pontual.
struct PointLight {
    vec3  pos{0.f, 5.f, 5.f};
    vec3  color{1.f, 1.f, 1.f};
    bool  enabled{true};
    float ambientScale{1.0f};
};

// Objeto de cena.
struct SceneObject {
    string  name;
    GLuint  VAO{0};
    int     nVerts{0};
    Material mat;
    GLuint   texID{0};

    // [TRANSFORMAÇÕES] acumuladas em graus / unidades.
    vec3  position{0,0,0};
    vec3  angles{0,0,0};
    vec3  scale{1,1,1};
    bool  rotX{false}, rotY{false}, rotZ{false};

    // [BÉZIER] pontos de controle.
    vector<vec3> bezierCPs;
    float        bezierT{0.f};
    float        bezierSpeed{0.f};
};

// Câmera FPS.
enum CamDir { CAM_FWD, CAM_BWD, CAM_LEFT, CAM_RIGHT };

class Camera {
public:
    vec3  pos{0,2,11};
    vec3  front, up, right;
    vec3  worldUp{0,1,0};
    float yaw{-90.f}, pitch{0.f};
    float speed{8.f}, sens{0.1f};

    Camera() { rebuild(); }
    Camera(vec3 p, float y, float pi) : pos(p), yaw(y), pitch(pi) { rebuild(); }

    mat4 getView() const { return lookAt(pos, pos + front, up); }

    void move(CamDir d, float dt) {
        float v = speed * dt;
        if (d == CAM_FWD)   pos += front * v;
        if (d == CAM_BWD)   pos -= front * v;
        if (d == CAM_LEFT)  pos -= right * v;
        if (d == CAM_RIGHT) pos += right * v;
    }

    // [CÂMERA] yaw/pitch a partir de offsets do mouse.
    void look(float dx, float dy) {
        yaw   += dx * sens;
        pitch  = glm::clamp(pitch + dy * sens, -89.f, 89.f);
        rebuild();
    }

private:
    // [CÂMERA].
    void rebuild() {
        vec3 f;
        f.x   = cos(radians(yaw))  * cos(radians(pitch));
        f.y   = sin(radians(pitch));
        f.z   = sin(radians(yaw))  * cos(radians(pitch));
        front = normalize(f);
        right = normalize(cross(front, worldUp));
        up    = normalize(cross(right, front));
    }
};

// Vertex Shader.
const GLchar* vsSource = R"(
#version 400

// [ATRIBUTOS].
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

// [UNIFORMS – MATRIZES].
uniform mat4 model;       // T*R*S do objeto
uniform mat4 view;        // lookAt da câmera FPS
uniform mat4 projection;  // perspective(fov, aspect, near, far)

out vec2 texCoord;
out vec3 vNormal;
out vec3 fragPos;

void main()
{
    // [MODEL → WORLD] aplica T*R*S.
    vec4 worldPos   = model * vec4(aPos, 1.0);

    // [MVP] posição final na tela.
    gl_Position = projection * view * worldPos;

    fragPos  = vec3(worldPos);
    texCoord = aTexCoord;

    // [NORMAL MATRIX] inverso-transposto da model — mantém perpendicularidade sob escalas não-uniformes.
    vNormal = mat3(transpose(inverse(model))) * aNormal;
}
)";

// Fragment Shader — Phong 3 luzes, atenuação difusa e rim light.
const GLchar* fsSource = R"(
#version 400

in vec2 texCoord;
in vec3 vNormal;
in vec3 fragPos;

// [TEXTURA] unidade 0 – map_Kd do .MTL.
uniform sampler2D texBuff;
// showTexture = 0: demonstra material sem mapa (Ka/Kd/Ks puros)
uniform int showTexture;

// [MATERIAL] coeficientes lidos do .MTL e enviados como uniforms.
uniform vec3  Ka;   // reflexão ambiente
uniform vec3  Kd;   // reflexão difusa
uniform vec3  Ks;   // reflexão especular
uniform float Ns;   // expoente de brilho

// [CÂMERA] posição no mundo (vetor V de Phong).
uniform vec3 camPos;

// [3 LUZES PONTUAIS].
uniform vec3  lightPos[3];           // posições no espaço mundo.
uniform vec3  lightColor[3];         // cor × intensidade.
uniform int   lightEnabled[3];       // toggle (teclas 1/2/3).
uniform float lightAmbientScale[3];  // 1=normal, 0=rim-only (back light).

// Constantes de atenuação quadrática.
const float attC = 1.0;
const float attL = 0.09;
const float attQ = 0.032;

out vec4 fragColor;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(camPos - fragPos);

    // [TEXTURA] amostra mapa ou branco (tecla 4).
    vec4 texColor = (showTexture != 0) ? texture(texBuff, texCoord) : vec4(1.0);

    vec3 result = vec3(0.0);

    // [LOOP DE ILUMINAÇÃO] acumula contribuição das 3 fontes.
    for (int i = 0; i < 3; i++)
    {
        if (lightEnabled[i] == 0) continue;

        vec3  L    = normalize(lightPos[i] - fragPos);
        vec3  R    = normalize(reflect(-L, N));
        float dist = length(lightPos[i] - fragPos);

        // [ATENUAÇÃO QUADRÁTICA].
        float att = 1.0 / (attC + attL * dist + attQ * dist * dist);

        // [PHONG – AMBIENTE].
        vec3 ambient  = Ka * lightColor[i] * lightAmbientScale[i];

        // [PHONG – DIFUSA].
        float diff   = max(dot(N, L), 0.0);
        vec3 diffuse = Kd * diff * lightColor[i] * att;

        // [PHONG – ESPECULAR].
        float spec    = pow(max(dot(R, V), 0.0), Ns);
        vec3 specular = Ks * spec * lightColor[i];

        // [RIM LIGHT] back light.
        float rim    = pow(clamp(1.0 - dot(N, V), 0.0, 1.0), 2.5);
        vec3 rimTerm = lightColor[i] * rim * (1.0 - lightAmbientScale[i]);

        result += ambient + diffuse + specular + rimTerm;
    }

    // [SAÍDA] textura modula iluminação total.
    fragColor = vec4(result * vec3(texColor), texColor.a);
}
)";

// Estado global.
Camera              gCam;
vector<SceneObject> gObjects;
PointLight          gLights[3];

int   gSelected    = 0;
bool  gCameraMode  = true;
bool  gPaused      = false;
bool  gShowTexture = true;

enum TMode { TM_ROTATE, TM_TRANSLATE, TM_SCALE };
TMode gTMode = TM_ROTATE;

float gLastX = WIN_W/2.f, gLastY = WIN_H/2.f;
bool  gFirstMouse = true;
float gFov = 45.f, gNear = 0.1f, gFar = 200.f;

// Protótipos.
void     key_callback(GLFWwindow*, int, int, int, int);
void     mouse_callback(GLFWwindow*, double, double);
int      setupShader();
GLuint   loadSimpleOBJ(const string&, int&, Material&);
GLuint   loadTexture(const string&);
GLuint   createWhiteTexture();
Material loadMTL(const string&);
void     parseScene(const string&);
void     writeDefaultScene(const string&);
vec3     evalBezier(const vector<vec3>&, float);

// [BÉZIER CÚBICO].
vec3 evalBezier(const vector<vec3>& cp, float t)
{
    if (cp.size() < 4) return cp.empty() ? vec3(0) : cp[0];
    int   nSegs = (int)(cp.size() / 4);
    float tmod  = fmod(t, (float)nSegs);
    int   seg   = (int)tmod;
    float u     = tmod - (float)seg;
    int   b     = seg * 4;
    vec3 P0 = cp[b], P1 = cp[b+1], P2 = cp[b+2], P3 = cp[b+3];
    float s = 1.f - u;
    return s*s*s*P0 + 3.f*s*s*u*P1 + 3.f*s*u*u*P2 + u*u*u*P3;
}

// loadMTL — lê Ka, Kd, Ks, Ns e map_Kd do .MTL
Material loadMTL(const string& mtlPath)
{
    Material mat;
    ifstream f(mtlPath);
    if (!f.is_open()) { cerr << "  MTL nao encontrado: " << mtlPath << endl; return mat; }
    string line;
    while (getline(f, line)) {
        istringstream ss(line); string tok; ss >> tok;
        if      (tok == "Ka")     ss >> mat.Ka.r     >> mat.Ka.g     >> mat.Ka.b;
        else if (tok == "Kd")     ss >> mat.Kd.r     >> mat.Kd.g     >> mat.Kd.b;
        else if (tok == "Ks")     ss >> mat.Ks.r     >> mat.Ks.g     >> mat.Ks.b;
        else if (tok == "Ns")     ss >> mat.Ns;
        else if (tok == "map_Kd") ss >> mat.textureName;
    }
    cout << "    Ka=(" << mat.Ka.r << "," << mat.Ka.g << "," << mat.Ka.b
         << ")  Kd=(" << mat.Kd.r << "," << mat.Kd.g << "," << mat.Kd.b
         << ")  Ns=" << mat.Ns << "  tex='" << mat.textureName << "'" << endl;
    return mat;
}

// Parser OBJ com fan-triangulation para quads.
GLuint loadSimpleOBJ(const string& filePath, int& nVertices, Material& mat)
{
    vector<vec3>    positions;
    vector<vec2>    texCoords;
    vector<vec3>    normals;
    vector<GLfloat> vBuffer;

    // Diretório base para localizar o .MTL.
    string baseDir;
    size_t sl = filePath.find_last_of("/\\");
    if (sl != string::npos) baseDir = filePath.substr(0, sl + 1);

    ifstream f(filePath);
    if (!f.is_open()) { cerr << "  OBJ nao encontrado: " << filePath << endl; return (GLuint)-1; }

    string line;
    while (getline(f, line))
    {
        istringstream ss(line); string tok; ss >> tok;
        if      (tok == "mtllib") { string mf; ss >> mf; mat = loadMTL(baseDir + mf); }
        else if (tok == "v")  { vec3 p; ss >> p.x >> p.y >> p.z; positions.push_back(p); }
        else if (tok == "vt") { vec2 t; ss >> t.s >> t.t; texCoords.push_back(t); }
        else if (tok == "vn") { vec3 n; ss >> n.x >> n.y >> n.z; normals.push_back(n); }
        else if (tok == "f")
        {
            // Coleta todos os vértices da face.
            vector<tuple<int,int,int>> face;
            string ft;
            while (ss >> ft) {
                int vi=0, ti=0, ni=0;
                istringstream fs(ft); string idx;
                if (getline(fs, idx, '/')) vi = !idx.empty() ? stoi(idx)-1 : 0;
                if (getline(fs, idx, '/')) ti = !idx.empty() ? stoi(idx)-1 : 0;
                if (getline(fs, idx))      ni = !idx.empty() ? stoi(idx)-1 : 0;
                face.push_back({vi, ti, ni});
            }
            // [TRIANGULAÇÃO FAN] suporta triângulos e quads.
            auto push = [&](int vi, int ti, int ni) {
                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);
                vBuffer.push_back(texCoords.empty() ? 0.f : texCoords[ti].s);
                vBuffer.push_back(texCoords.empty() ? 0.f : texCoords[ti].t);
                vBuffer.push_back(normals.empty()   ? 0.f : normals[ni].x);
                vBuffer.push_back(normals.empty()   ? 0.f : normals[ni].y);
                vBuffer.push_back(normals.empty()   ? 1.f : normals[ni].z);
            };
            for (int i = 1; i+1 < (int)face.size(); i++) {
                auto [v0,t0,n0] = face[0];
                auto [vi,ti,ni] = face[i];
                auto [vj,tj,nj] = face[i+1];
                push(v0,t0,n0); push(vi,ti,ni); push(vj,tj,nj);
            }
        }
    }

    // [VBO] Envia vértices para GPU.
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size()*sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    // [VAO] layout.
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    const GLsizei stride = 8 * sizeof(GLfloat);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(GLfloat)));  glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5*sizeof(GLfloat)));  glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 8);
    cout << "  '" << filePath << "' → " << nVertices << " verts" << endl;
    return VAO;
}

// Textura 1×1 branca (fallback sem map_Kd).
GLuint createWhiteTexture()
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    unsigned char white[3] = {255,255,255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

// Carrega textura.
GLuint loadTexture(const string& filePath)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int w, h, nc;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filePath.c_str(), &w, &h, &nc, 0);
    if (data) {
        GLenum fmt = (nc == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        cout << "  Textura '" << filePath << "' (" << w << "x" << h << ")" << endl;
    } else {
        cerr << "  Falha textura '" << filePath << "' → fallback branco" << endl;
        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);
        return createWhiteTexture();
    }
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

// Compila VS + FS e linka o programa.
int setupShader()
{
    GLint ok; GLchar log[512];
    auto compile = [&](GLenum type, const GLchar* src, const char* label) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { glGetShaderInfoLog(s,512,nullptr,log); cerr << label << ":\n" << log << endl; }
        return s;
    };
    GLuint vs   = compile(GL_VERTEX_SHADER,   vsSource, "VERTEX");
    GLuint fs   = compile(GL_FRAGMENT_SHADER, fsSource, "FRAGMENT");
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog,512,nullptr,log); cerr << "LINK:\n" << log << endl; }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// Gera assets/scene.txt padrão se ausente.
void writeDefaultScene(const string& path)
{
    ofstream f(path);
    if (!f.is_open()) { cerr << "Nao foi possivel criar " << path << endl; return; }
    f << "# GrauB - Configuracao da Cena\n"
         "# Autor: Adriano Fantinelli\n"
         "#\n"
         "# Sintaxe:\n"
         "#   camera   px py pz  yaw pitch\n"
         "#   frustum  fov near far\n"
         "#   light    id  px py pz  cr cg cb  ambientScale\n"
         "#   object   name objPath  px py pz  rx ry rz  scale  bezierSpeed\n"
         "#   bezier   name  p0x p0y p0z  p1x p1y p1z  p2x p2y p2z  p3x p3y p3z\n"
         "\n"
         "# [CAMERA] posicao inicial e orientacao (yaw e pitch em graus).\n"
         "camera  0.0  2.0  11.0  -90.0  -8.0\n"
         "\n"
         "# [FRUSTUM] campo de visao, plano proximo, plano distante.\n"
         "frustum  45.0  0.1  200.0\n"
         "\n"
         "# [LUZES] iluminacao de 3 pontos.\n"
         "# Key  light: frente/esquerda/acima, branca 100%.\n"
         "light 0  -5.0  7.0  7.0   1.0 1.0 1.0   1.0\n"
         "# Fill light: frente/direita/leve-acima, branca 50%.\n"
         "light 1   5.0  4.0  7.0   0.5 0.5 0.5   1.0\n"
         "# Back light: atras/acima, azul-fria, rim-only.\n"
         "light 2   0.0  4.0 -7.0   0.8 0.9 2.0   0.0\n"
         "\n"
         "# [OBJETOS]  name  objPath  px py pz  rx ry rz  scale  bezierSpeed\n"
         "object Pedestal   ../assets/Modelos3D/Cube.obj            3.5  -1.5  -1.5   0.0  30.0  0.0   1.0   0.0\n"
         "object SuzanneHD  ../assets/Modelos3D/SuzanneSubdiv1.obj  3.5   0.5  -1.5   0.0  15.0  0.0   0.9   0.0\n"
         "object Suzanne    ../assets/Modelos3D/Suzanne.obj          0.0   0.6   0.0   0.0   0.0  0.0   0.8   0.4\n"
         "\n"
         "# [BEZIER] trajetoria oval de Suzanne (2 segmentos cubicos = loop fechado).\n"
         "bezier Suzanne   0.0 0.6  0.0   3.5 1.6  0.0   3.5 1.6 -5.0   0.0 0.6 -5.0\n"
         "bezier Suzanne   0.0 0.6 -5.0  -3.5 1.6 -5.0  -3.5 1.6  0.0   0.0 0.6  0.0\n";
    cout << "[scene] Arquivo gerado: " << path << endl;
}

// [PARSER DA CENA] lê assets/scene.txt linha a linha.
void parseScene(const string& path)
{
    // Gera arquivo padrão se não existir.
    { ifstream t(path); if (!t.is_open()) writeDefaultScene(path); }

    ifstream f(path);
    if (!f.is_open()) { cerr << "[parseScene] Nao encontrou: " << path << endl; return; }

    cout << "[parseScene] Lendo: " << path << endl;
    string line;
    while (getline(f, line))
    {
        // Ignora linhas vazias e comentários.
        if (line.empty() || line[0] == '#') continue;

        // Remove comentário inline.
        size_t hashPos = line.find('#');
        if (hashPos != string::npos) line = line.substr(0, hashPos);

        istringstream ss(line); string cmd; ss >> cmd;
        if (cmd.empty()) continue;

        // [CAMERA] posicao e orientacao inicial da camera.
        if (cmd == "camera") {
            vec3 p; float y, pi;
            ss >> p.x >> p.y >> p.z >> y >> pi;
            gCam = Camera(p, y, pi);
            cout << "  Camera pos=(" << p.x << "," << p.y << "," << p.z
                 << ") yaw=" << y << " pitch=" << pi << endl;
        }
        // [FRUSTUM] parametros da projecao perspectiva.
        else if (cmd == "frustum") {
            ss >> gFov >> gNear >> gFar;
            cout << "  Frustum fov=" << gFov << " near=" << gNear << " far=" << gFar << endl;
        }
        // [LUZES].
        else if (cmd == "light") {
            int id; ss >> id;
            if (id < 0 || id > 2) continue;
            ss >> gLights[id].pos.x   >> gLights[id].pos.y   >> gLights[id].pos.z;
            ss >> gLights[id].color.r >> gLights[id].color.g >> gLights[id].color.b;
            ss >> gLights[id].ambientScale;
            cout << "  Luz[" << id << "] pos=(" << gLights[id].pos.x
                 << "," << gLights[id].pos.y << "," << gLights[id].pos.z
                 << ")  ambScale=" << gLights[id].ambientScale << endl;
        }
        // [OBJETO].
        else if (cmd == "object") {
            SceneObject obj;
            string objPath;
            float rx, ry, rz, sc, spd;
            ss >> obj.name >> objPath
               >> obj.position.x >> obj.position.y >> obj.position.z
               >> rx >> ry >> rz >> sc >> spd;
            obj.angles      = vec3(rx, ry, rz);
            obj.scale       = vec3(sc);
            obj.bezierSpeed = spd;

            cout << "  Objeto '" << obj.name << "' <- " << objPath << endl;
            obj.VAO = loadSimpleOBJ(objPath, obj.nVerts, obj.mat);
            if (obj.VAO == (GLuint)-1) { cerr << "  ERRO: " << objPath << endl; continue; }

            // Carrega textura (fallback branco se map_Kd vazio).
            obj.texID = obj.mat.textureName.empty()
                      ? createWhiteTexture()
                      : loadTexture("../assets/Modelos3D/" + obj.mat.textureName);

            gObjects.push_back(obj);
        }
        // [BEZIER] name + 12 floats.
        else if (cmd == "bezier") {
            string name; ss >> name;
            vector<float> vals; float v;
            while (ss >> v) vals.push_back(v);
            if ((int)vals.size() < 12) {
                cerr << "  bezier '" << name << "': precisa de 12 floats por segmento" << endl;
                continue;
            }
            for (auto& obj : gObjects) {
                if (obj.name == name) {
                    for (int i = 0; i+2 < (int)vals.size(); i += 3)
                        obj.bezierCPs.push_back(vec3(vals[i], vals[i+1], vals[i+2]));
                    break;
                }
            }
        }
    }
    cout << "[parseScene] " << gObjects.size() << " objeto(s) carregado(s)." << endl;
}

// Eventos discretos de teclado.
void key_callback(GLFWwindow* window, int key, int /*sc*/, int action, int /*mode*/)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if (action != GLFW_PRESS) return;

    // [M] Toggle entre modo câmera e objeto.
    if (key == GLFW_KEY_M) {
        gCameraMode = !gCameraMode;
        glfwSetInputMode(window, GLFW_CURSOR,
            gCameraMode ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        gFirstMouse = true;
        cout << (gCameraMode ? "[CAMERA] WASD + mouse" : "[OBJETO] Tab / R / T / S") << endl;
    }

    // [1/2/3] Toggle das 3 fontes de luz.
    if (key == GLFW_KEY_1) { gLights[0].enabled = !gLights[0].enabled; cout << "Key  light: " << (gLights[0].enabled?"ON":"OFF") << endl; }
    if (key == GLFW_KEY_2) { gLights[1].enabled = !gLights[1].enabled; cout << "Fill light: " << (gLights[1].enabled?"ON":"OFF") << endl; }
    if (key == GLFW_KEY_3) { gLights[2].enabled = !gLights[2].enabled; cout << "Back light: " << (gLights[2].enabled?"ON":"OFF") << endl; }

    // [4] Toggle textura / material puro.
    if (key == GLFW_KEY_4) {
        gShowTexture = !gShowTexture;
        cout << "Textura: " << (gShowTexture ? "ON (map_Kd UV)" : "OFF (Ka/Kd/Ks puro)") << endl;
    }

    // [Space] Pausar / retomar animações Bézier.
    if (key == GLFW_KEY_SPACE) {
        gPaused = !gPaused;
        cout << "Animacao Bezier: " << (gPaused ? "PAUSADA" : "RODANDO") << endl;
    }

    // Controles de objeto — só no modo objeto.
    if (gCameraMode) return;

    // [Tab] Selecionar próximo objeto.
    if (key == GLFW_KEY_TAB && !gObjects.empty()) {
        gSelected = (gSelected + 1) % (int)gObjects.size();
        cout << "Selecionado: [" << gSelected << "] " << gObjects[gSelected].name << endl;
    }

    // [R / T / S] Modo de transformação.
    if (key == GLFW_KEY_R) { gTMode = TM_ROTATE;    cout << "Modo: ROTACAO  (X/Y/Z)" << endl; }
    if (key == GLFW_KEY_T) { gTMode = TM_TRANSLATE;  cout << "Modo: TRANSLACAO (setas, I/K)" << endl; }
    if (key == GLFW_KEY_S) { gTMode = TM_SCALE;      cout << "Modo: ESCALA  (= e -)" << endl; }

    // [X / Y / Z] Toggle de rotação contínua no eixo (modo rotação).
    if (gTMode == TM_ROTATE && !gObjects.empty()) {
        auto& o = gObjects[gSelected];
        if (key == GLFW_KEY_X) { o.rotX = !o.rotX; o.rotY = false; o.rotZ = false; cout << "rotX: " << o.rotX << endl; }
        if (key == GLFW_KEY_Y) { o.rotX = false; o.rotY = !o.rotY; o.rotZ = false; cout << "rotY: " << o.rotY << endl; }
        if (key == GLFW_KEY_Z) { o.rotX = false; o.rotY = false; o.rotZ = !o.rotZ; cout << "rotZ: " << o.rotZ << endl; }
    }
}

// yaw/pitch da câmera FPS.
void mouse_callback(GLFWwindow* /*w*/, double xPos, double yPos)
{
    if (!gCameraMode) return;
    if (gFirstMouse) { gLastX = (float)xPos; gLastY = (float)yPos; gFirstMouse = false; return; }
    float dx =  (float)xPos - gLastX;
    float dy = gLastY - (float)yPos;
    gLastX = (float)xPos;  gLastY = (float)yPos;
    gCam.look(dx, dy);
}

int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIN_W, WIN_H,
        "GrauB — Diorama 3D  |  M=cam/obj  1/2/3=luz  4=tex  Tab=sel  R/T/S=transf  Space=anim",
        nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "GLAD falhou" << endl; return -1;
    }
    cout << "Renderer: " << glGetString(GL_RENDERER) << "\n"
         << "OpenGL:   " << glGetString(GL_VERSION)  << "\n" << endl;

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    // Defaults de luzes.
    gLights[0] = { vec3(-5.f, 7.f, 7.f), vec3(1.0f,1.0f,1.0f), true, 1.0f };
    gLights[1] = { vec3( 5.f, 4.f, 7.f), vec3(0.5f,0.5f,0.5f), true, 1.0f };
    gLights[2] = { vec3( 0.f, 4.f,-7.f), vec3(0.8f,0.9f,2.0f), true, 0.0f };

    // [PARSER DA CENA] lê (ou gera) assets/scene.txt.
    parseScene("../assets/scene.txt");

    if (gObjects.empty()) {
        cerr << "Nenhum objeto carregado. Verifique assets/scene.txt" << endl;
        glfwTerminate(); return -1;
    }

    GLuint shaderID = setupShader();
    glUseProgram(shaderID);

    // [UNIFORM] texBuff sempre na unidade de textura 0.
    glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);

    // [PROJEÇÃO] perspective(fov, aspect, near, far) — frustum da cena.
    mat4 projection = perspective(radians(gFov), (float)WIN_W/(float)WIN_H, gNear, gFar);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));

    // [LUZES – UNIFORMS ESTÁTICOS] posição, cor e ambientScale não mudam em runtime.
    for (int i = 0; i < 3; i++) {
        string si = to_string(i);
        glUniform3fv(glGetUniformLocation(shaderID, ("lightPos["          + si + "]").c_str()), 1, value_ptr(gLights[i].pos));
        glUniform3fv(glGetUniformLocation(shaderID, ("lightColor["        + si + "]").c_str()), 1, value_ptr(gLights[i].color));
        glUniform1f (glGetUniformLocation(shaderID, ("lightAmbientScale[" + si + "]").c_str()), gLights[i].ambientScale);
    }

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);

    cout << "\n=== CONTROLES ===" << endl;
    cout << "  M          — camera FPS / modo objeto." << endl;
    cout << "  WASD+mouse — mover/rotacionar camera (modo camera)." << endl;
    cout << "  Tab        — proximo objeto  |  R/T/S — rotacao/translacao/escala." << endl;
    cout << "  X/Y/Z      — toggle eixo rot  |  Setas/I/K — transladar  |  =/- — escalar." << endl;
    cout << "  1/2/3      — toggle key/fill/back light." << endl;
    cout << "  4          — toggle textura UV vs material puro." << endl;
    cout << "  Space      — pausar/retomar Bezier." << endl;
    cout << "  ESC        — sair\n" << endl;

    float lastFrame = 0.f;

    // Loop principal de renderização.
    while (!glfwWindowShouldClose(window))
    {
        float now = (float)glfwGetTime();
        float dt  = now - lastFrame;
        lastFrame = now;

        glfwPollEvents();

        // [CÂMERA] movimento contínuo via polling — modo câmera.
        if (gCameraMode) {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) gCam.move(CAM_FWD,   dt);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) gCam.move(CAM_BWD,   dt);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) gCam.move(CAM_LEFT,  dt);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) gCam.move(CAM_RIGHT, dt);
        }

        // [TRANSLAÇÃO] setas (XY) e I/K (Z) — modo objeto + translação.
        if (!gCameraMode && gTMode == TM_TRANSLATE && !gObjects.empty()) {
            auto& o = gObjects[gSelected];
            float v = TRANS_V * dt;
            if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) o.position.y += v;
            if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) o.position.y -= v;
            if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) o.position.x -= v;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) o.position.x += v;
            if (glfwGetKey(window, GLFW_KEY_I)     == GLFW_PRESS) o.position.z -= v;
            if (glfwGetKey(window, GLFW_KEY_K)     == GLFW_PRESS) o.position.z += v;
        }

        // [ESCALA UNIFORME] = aumenta, - diminui — modo objeto + escala.
        if (!gCameraMode && gTMode == TM_SCALE && !gObjects.empty()) {
            auto& o  = gObjects[gSelected];
            float v  = SCALE_V * dt;
            bool up  = glfwGetKey(window, GLFW_KEY_EQUAL)       == GLFW_PRESS
                    || glfwGetKey(window, GLFW_KEY_KP_ADD)       == GLFW_PRESS;
            bool dn  = glfwGetKey(window, GLFW_KEY_MINUS)        == GLFW_PRESS
                    || glfwGetKey(window, GLFW_KEY_KP_SUBTRACT)  == GLFW_PRESS;
            if (up) o.scale = glm::min(o.scale + v, vec3(10.f));
            if (dn) o.scale = glm::max(o.scale - v, vec3(0.05f));
        }

        // [ANIMAÇÃO BÉZIER] avança t dos objetos animados.
        if (!gPaused)
            for (auto& o : gObjects)
                if (o.bezierCPs.size() >= 4 && o.bezierSpeed > 0.f)
                    o.bezierT += o.bezierSpeed * dt;

        // [ROTAÇÃO ACUMULADA] acumula ângulos dos objetos com eixo ativo.
        for (auto& o : gObjects) {
            if (o.rotX) o.angles.x += ROT_V * dt;
            if (o.rotY) o.angles.y += ROT_V * dt;
            if (o.rotZ) o.angles.z += ROT_V * dt;
        }

        glClearColor(0.07f, 0.07f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // [VIEW MATRIX] atualiza câmera FPS a cada frame.
        mat4 view = gCam.getView();
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"),    1, GL_FALSE, value_ptr(view));
        glUniform3fv       (glGetUniformLocation(shaderID, "camPos"), 1, value_ptr(gCam.pos));

        // [LUZES ENABLED] atualizado por frame (teclas 1/2/3).
        for (int i = 0; i < 3; i++)
            glUniform1i(glGetUniformLocation(shaderID, ("lightEnabled[" + to_string(i) + "]").c_str()),
                        gLights[i].enabled ? 1 : 0);

        // [TEXTURA TOGGLE] tecla 4.
        glUniform1i(glGetUniformLocation(shaderID, "showTexture"), gShowTexture ? 1 : 0);

        // Renderiza cada objeto da cena.
        for (int i = 0; i < (int)gObjects.size(); i++)
        {
            auto& o = gObjects[i];

            vec3 pos = (o.bezierCPs.size() >= 4 && o.bezierSpeed > 0.f)
                     ? evalBezier(o.bezierCPs, o.bezierT)
                     : o.position;

            mat4 model = mat4(1.f);
            model = translate(model,   pos);
            model = rotate(model, radians(o.angles.x), vec3(1,0,0));
            model = rotate(model, radians(o.angles.y), vec3(0,1,0));
            model = rotate(model, radians(o.angles.z), vec3(0,0,1));
            model = glm::scale(model, o.scale);

            // [UNIFORM – MODEL] envia ao vertex shader.
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));

            // [UNIFORMS – MATERIAL] Ka/Kd/Ks/Ns do .MTL
            // Objeto selecionado (modo objeto) recebe boost de Ka como destaque visual.
            float boost = (!gCameraMode && i == gSelected) ? 2.0f : 1.0f;
            vec3 ka = o.mat.Ka * boost;
            glUniform3fv(glGetUniformLocation(shaderID, "Ka"), 1, value_ptr(ka));
            glUniform3fv(glGetUniformLocation(shaderID, "Kd"), 1, value_ptr(o.mat.Kd));
            glUniform3fv(glGetUniformLocation(shaderID, "Ks"), 1, value_ptr(o.mat.Ks));
            glUniform1f (glGetUniformLocation(shaderID, "Ns"), o.mat.Ns);

            // [TEXTURA] bind da textura do objeto corrente (unidade 0).
            glBindTexture(GL_TEXTURE_2D, o.texID);

            glBindVertexArray(o.VAO);
            glDrawArrays(GL_TRIANGLES, 0, o.nVerts);
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    for (auto& o : gObjects) glDeleteVertexArrays(1, &o.VAO);
    glfwTerminate();
    return 0;
}
