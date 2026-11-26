#include <iostream>
#include <vector>
#include <string>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

// Shaders principales del avion (posiciones, normales, texturas e iluminacion Phong)
const char* vShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
out vec3 FragPos, Normal;
out vec2 TexCoord;
uniform mat4 model, view, projection;
void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
})";

const char* fShaderSrc = R"(
#version 330 core
out vec4 FragColor;
in vec3 FragPos, Normal;
in vec2 TexCoord;
uniform vec3 lightPos, viewPos, objectColor, lightColor;
uniform bool useTexture;
uniform sampler2D texture_diffuse;
void main() {
    // Mezclo la textura original con el color elegido en el panel
    vec3 baseColor = useTexture ? texture(texture_diffuse, TexCoord).rgb * objectColor : objectColor;

    // Luz ambiental, difusa y especular (modelo Phong)
    vec3 ambient = 0.45 * lightColor;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 diffuse = max(dot(norm, lightDir), 0.0) * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = 0.55 * spec * lightColor;

    FragColor = vec4((ambient + diffuse + specular) * baseColor, 1.0);
})";

// Shaders del fondo 3D (Skybox cubemap)
const char* skyVShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 TexCoords;
uniform mat4 projection, view;
void main() {
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
})";

const char* skyFShaderSrc = R"(
#version 330 core
out vec4 FragColor;
in vec3 TexCoords;
uniform samplerCube skybox;
void main() { FragColor = texture(skybox, TexCoords); })";

// Shaders 2D para dibujar los botones del panel
const char* uiVShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform mat4 projection;
void main() { gl_Position = projection * vec4(aPos, 0.0, 1.0); })";

const char* uiFShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uiColor;
void main() { FragColor = uiColor; })";

// Configuracion de ventana, camara y controles
const int WIDTH = 1000, HEIGHT = 800;
glm::vec3 camPos(0.0f, 3.0f, 10.0f), camFront(0.0f, -0.25f, -1.0f), camUp(0.0f, 1.0f, 0.0f);
float yaw = -90.0f, pitch = -14.0f, lastX = WIDTH / 2.0f, lastY = HEIGHT / 2.0f;
bool firstMouse = true, dragging = false;
float deltaTime = 0.0f, lastFrame = 0.0f;

// Control manual de posicion en la curva de Bezier (t va de 0.0 a 1.0)
float tBezier = 0.2f;
int dirBotonBezier = 0; // -1 = retroceder, 0 = quieto, 1 = avanzar
bool mirandoAtras = false;

// Variables para rotar el avion con teclas y cambiarle el color
float rotYManual = 0.0f, rotZManual = 0.0f;
glm::vec3 colorAvion(1.0f, 1.0f, 1.0f);

struct BotonUI {
    float x, y, w, h;
    glm::vec3 color;
    std::string nombre;
    int accion; // 0 = color, -1 = retroceder en curva, 1 = avanzar en curva
};

// Fila 1: 5 colores para el avion | Fila 2: 2 botones para moverlo por la curva
std::vector<BotonUI> botones = {
    { 20.0f,  48.0f, 32.0f, 30.0f, {1.00f, 1.00f, 1.00f}, "Blanco Original", 0 },
    { 62.0f,  48.0f, 32.0f, 30.0f, {1.00f, 0.30f, 0.30f}, "Rojo",            0 },
    { 104.0f, 48.0f, 32.0f, 30.0f, {0.35f, 0.65f, 1.00f}, "Azul",            0 },
    { 146.0f, 48.0f, 32.0f, 30.0f, {0.35f, 0.95f, 0.55f}, "Verde",           0 },
    { 188.0f, 48.0f, 32.0f, 30.0f, {1.00f, 0.85f, 0.25f}, "Dorado",          0 },
    { 20.0f,  90.0f, 95.0f, 28.0f, {0.85f, 0.35f, 0.35f}, "Retroceder (<-)", -1 },
    { 125.0f, 90.0f, 95.0f, 28.0f, {0.30f, 0.75f, 0.45f}, "Avanzar (->)",     1 }
};

// Movimiento de camara (WASD), traslado en curva (Flechas) y rotacion manual (Q, E, R)
void procesarTeclado(GLFWwindow* win) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, true);

    float velCam = 6.5f * deltaTime;
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) camPos += velCam * camFront;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) camPos -= velCam * camFront;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) camPos -= glm::normalize(glm::cross(camFront, camUp)) * velCam;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) camPos += glm::normalize(glm::cross(camFront, camUp)) * velCam;
    if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) camPos += velCam * camUp;
    if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camPos -= velCam * camUp;

    // Trasladar el avion por la curva con flechas o manteniendo presionados los botones
    float velCurva = 0.35f * deltaTime;
    if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS || dirBotonBezier == 1) {
        tBezier = glm::clamp(tBezier + velCurva, 0.0f, 1.0f);
        mirandoAtras = false;
    }
    if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS || dirBotonBezier == -1) {
        tBezier = glm::clamp(tBezier - velCurva, 0.0f, 1.0f);
        mirandoAtras = true;
    }

    // Rotacion extra del avion sobre su propio eje
    float velRot = 90.0f * deltaTime;
    if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) rotZManual -= velRot;
    if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) rotZManual += velRot;
    if (glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS) rotYManual += velRot * 1.5f;
}

// Girar vista 360 grados solo cuando mantengo presionado el clic izquierdo fuera del panel
void mouseMoveCallback(GLFWwindow* win, double xpos, double ypos) {
    if (!dragging) { firstMouse = true; return; }
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }

    yaw += ((float)xpos - lastX) * 0.3f;
    pitch += (lastY - (float)ypos) * 0.3f;
    lastX = (float)xpos; lastY = (float)ypos;

    pitch = glm::clamp(pitch, -89.0f, 89.0f);
    camFront = glm::normalize(glm::vec3(
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch))
    ));
}

// Detectar clic en los botones del panel (colores o movimiento) o activar giro de camara
void mouseClickCallback(GLFWwindow* win, int btn, int action, int mods) {
    if (btn != GLFW_MOUSE_BUTTON_LEFT) return;
    if (action == GLFW_RELEASE) {
        dragging = false;
        dirBotonBezier = 0; // Detiene el movimiento al soltar el boton del panel
        return;
    }

    double x, y;
    glfwGetCursorPos(win, &x, &y);

    // Reviso si hizo clic dentro del panel superior izquierdo
    if (x >= 10.0 && x <= 232.0 && y >= 15.0 && y <= 130.0) {
        for (const auto& b : botones) {
            if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) {
                if (b.accion == 0) {
                    colorAvion = b.color;
                    std::cout << "Color cambiado a: " << b.nombre << std::endl;
                }
                else {
                    dirBotonBezier = b.accion; // -1 retrocede, 1 avanza mientras mantenga clic
                }
            }
        }
    }
    else {
        dragging = true;
        firstMouse = true;
    }
}

// Puntos de control para la trayectoria curva de Bezier
const glm::vec3 P0(-4.0f, -1.0f, -2.0f), P1(-2.0f, 3.0f, 2.0f);
const glm::vec3 P2(2.0f, -3.0f, 2.0f), P3(4.0f, 1.0f, -2.0f);

glm::vec3 puntoBezier(float t) {
    float u = 1.0f - t;
    return (u * u * u * P0) + (3 * u * u * t * P1) + (3 * u * t * t * P2) + (t * t * t * P3);
}

glm::vec3 tangenteBezier(float t) {
    float u = 1.0f - t;
    return 3 * u * u * (P1 - P0) + 6 * u * t * (P2 - P1) + 3 * t * t * (P3 - P2);
}

// Funcion auxiliar para subir atributos (posicion, normal, UV) de TinyGLTF a OpenGL
void subirAtributoGLTF(const tinygltf::Model& m, const tinygltf::Primitive& p, const std::string& attr, GLuint loc, int size) {
    if (p.attributes.find(attr) == p.attributes.end()) return;
    const auto& acc = m.accessors[p.attributes.at(attr)];
    const auto& bv = m.bufferViews[acc.bufferView];
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, acc.count * size * sizeof(float), &m.buffers[bv.buffer].data[bv.byteOffset + acc.byteOffset], GL_STATIC_DRAW);
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, size, GL_FLOAT, GL_FALSE, size * sizeof(float), 0);
}

// Carga el modelo .glb y extrae su textura interna
GLuint meshVAO = 0, texAvion = 0;
bool tieneTextura = false;
size_t totalIndices = 0;
GLenum tipoIndice = GL_UNSIGNED_SHORT;

bool cargarAvionGLB(const std::string& ruta) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, ruta)) return false;

    const auto& prim = model.meshes[0].primitives[0];
    glGenVertexArrays(1, &meshVAO);
    glBindVertexArray(meshVAO);

    subirAtributoGLTF(model, prim, "POSITION", 0, 3);
    subirAtributoGLTF(model, prim, "NORMAL", 1, 3);
    subirAtributoGLTF(model, prim, "TEXCOORD_0", 2, 2);

    const auto& idxAcc = model.accessors[prim.indices];
    const auto& idxBv = model.bufferViews[idxAcc.bufferView];
    totalIndices = idxAcc.count;
    tipoIndice = idxAcc.componentType;
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxAcc.count * tinygltf::GetComponentSizeInBytes(tipoIndice),
        &model.buffers[idxBv.buffer].data[idxBv.byteOffset + idxAcc.byteOffset], GL_STATIC_DRAW);
    glBindVertexArray(0);

    if (prim.material >= 0) {
        int texIdx = model.materials[prim.material].pbrMetallicRoughness.baseColorTexture.index;
        if (texIdx >= 0) {
            const auto& img = model.images[model.textures[texIdx].source];
            glGenTextures(1, &texAvion);
            glBindTexture(GL_TEXTURE_2D, texAvion);
            GLenum fmt = (img.component == 3) ? GL_RGB : GL_RGBA;
            glTexImage2D(GL_TEXTURE_2D, 0, fmt, img.width, img.height, 0, fmt, img.pixel_type, img.image.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            tieneTextura = true;
        }
    }
    return true;
}

// Carga las 6 imagenes del Skybox en un Cubemap
GLuint cargarSkybox(const std::vector<std::string>& caras) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, id);
    int w, h, ch;
    for (unsigned int i = 0; i < caras.size(); i++) {
        unsigned char* data = stbi_load(caras[i].c_str(), &w, &h, &ch, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, w, h, 0, (ch == 4 ? GL_RGBA : GL_RGB), GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return id;
}

// Compila y enlaza un par de shaders
GLuint crearShader(const char* vSrc, const char* fSrc) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &vSrc, NULL); glCompileShader(vs);
    glShaderSource(fs, 1, &fSrc, NULL); glCompileShader(fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// Dibuja un rectangulo 2D para el panel de botones
GLuint uiVAO, uiVBO;
void dibujarCajaUI(GLuint shader, float x, float y, float w, float h, glm::vec4 col) {
    float verts[] = { x, y + h, x, y, x + w, y, x, y + h, x + w, y, x + w, y + h };
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glUniform4fv(glGetUniformLocation(shader, "uiColor"), 1, glm::value_ptr(col));
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Avion 3D - Control Manual Bezier + Skybox + Phong", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouseMoveCallback);
    glfwSetMouseButtonCallback(window, mouseClickCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint shaderAvion = crearShader(vShaderSrc, fShaderSrc);
    GLuint shaderSky = crearShader(skyVShaderSrc, skyFShaderSrc);
    GLuint shaderUI = crearShader(uiVShaderSrc, uiFShaderSrc);

    // Cubo base para el Skybox
    float skyVerts[] = {
        -1, 1,-1, -1,-1,-1,  1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
        -1,-1, 1, -1,-1,-1, -1, 1,-1, -1, 1,-1, -1, 1, 1, -1,-1, 1,
         1,-1,-1,  1,-1, 1,  1, 1, 1,  1, 1, 1,  1, 1,-1,  1,-1,-1,
        -1,-1, 1, -1, 1, 1,  1, 1, 1,  1, 1, 1,  1,-1, 1, -1,-1, 1,
        -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1,
        -1,-1,-1, -1,-1, 1,  1,-1,-1,  1,-1,-1, -1,-1, 1,  1,-1, 1
    };
    GLuint skyVAO, skyVBO;
    glGenVertexArrays(1, &skyVAO); glGenBuffers(1, &skyVBO);
    glBindVertexArray(skyVAO); glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyVerts), skyVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

    GLuint cubemapTex = cargarSkybox({
        "Skybox/right.jpg", "Skybox/left.jpg", "Skybox/top.jpg",
        "Skybox/bottom.jpg", "Skybox/front.jpg", "Skybox/back.jpg"
        });

    // Buffers para la interfaz 2D
    glGenVertexArrays(1, &uiVAO); glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);

    if (!cargarAvionGLB("Model/Airplane.glb")) {
        std::cerr << "No se encontro Model/Airplane.glb" << std::endl;
        return -1;
    }

    // Resumen de controles en consola
    std::cout << "Controles:\n"
        << "- Botones Rojo/Verde abajo en panel (o Flechas Izq/Der): Mover avion por la curva de Bezier\n"
        << "- Cuadros superiores del panel: Cambiar color del avion\n"
        << "- WASD + Espacio/Shift: Mover camara\n"
        << "- Clic Izquierdo + Arrastrar: Girar vista 360\n"
        << "- Q/E/R: Rotar avion manualmente\n";

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        deltaTime = now - lastFrame;
        lastFrame = now;
        procesarTeclado(window);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);

        // 1. Dibujar el avion en la posicion manual tBezier (sin mostrar la linea)
        glEnable(GL_DEPTH_TEST);
        glUseProgram(shaderAvion);
        glm::vec3 pos = puntoBezier(tBezier);
        glm::vec3 dir = glm::normalize(tangenteBezier(tBezier));
        float anguloY = std::atan2(dir.x, dir.z) + (mirandoAtras ? glm::radians(180.0f) : 0.0f);

        glUniform3f(glGetUniformLocation(shaderAvion, "lightPos"), 0.0f, 10.0f, 12.0f);
        glUniform3fv(glGetUniformLocation(shaderAvion, "viewPos"), 1, glm::value_ptr(camPos));
        glUniform3f(glGetUniformLocation(shaderAvion, "lightColor"), 1.0f, 1.0f, 1.0f);
        glUniform3fv(glGetUniformLocation(shaderAvion, "objectColor"), 1, glm::value_ptr(colorAvion));
        glUniform1i(glGetUniformLocation(shaderAvion, "useTexture"), tieneTextura);
        glUniformMatrix4fv(glGetUniformLocation(shaderAvion, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderAvion, "projection"), 1, GL_FALSE, glm::value_ptr(proj));

        glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
        model = glm::rotate(model, anguloY + glm::radians(rotYManual), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(rotZManual), glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(0.05f));
        glUniformMatrix4fv(glGetUniformLocation(shaderAvion, "model"), 1, GL_FALSE, glm::value_ptr(model));

        if (tieneTextura) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texAvion); }
        glBindVertexArray(meshVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)totalIndices, tipoIndice, 0);

        // 2. Dibujar el Skybox 360 grados
        glDepthFunc(GL_LEQUAL);
        glUseProgram(shaderSky);
        glm::mat4 skyView = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderSky, "view"), 1, GL_FALSE, glm::value_ptr(skyView));
        glUniformMatrix4fv(glGetUniformLocation(shaderSky, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glBindVertexArray(skyVAO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        // 3. Dibujar el panel 2D (colores arriba + controles de avance/retroceso abajo)
        glDisable(GL_DEPTH_TEST);
        glUseProgram(shaderUI);
        glm::mat4 ortho = glm::ortho(0.0f, (float)WIDTH, (float)HEIGHT, 0.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderUI, "projection"), 1, GL_FALSE, glm::value_ptr(ortho));

        dibujarCajaUI(shaderUI, 10, 15, 222, 115, { 0.1f, 0.1f, 0.12f, 0.78f });
        dibujarCajaUI(shaderUI, 10, 15, 222, 22, { 0.2f, 0.24f, 0.32f, 0.95f });
        for (const auto& b : botones) {
            dibujarCajaUI(shaderUI, b.x - 2, b.y - 2, b.w + 4, b.h + 4, { 1, 1, 1, 0.85f });
            dibujarCajaUI(shaderUI, b.x, b.y, b.w, b.h, glm::vec4(b.color, 1.0f));
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}