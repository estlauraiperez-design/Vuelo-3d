# Simulador de Vuelo 3D — OpenGL 3.3 Core

Aplicación gráfica interactiva desarrollada en **C++** y **OpenGL moderno (3.3 Core Profile)** que simula el desplazamiento de una aeronave 3D (`Airplane.glb`) sobre una trayectoria paramétrica de **Bézier cúbica**, rodeada por un entorno inmersivo de 360° (**Skybox Cubemap**), iluminación **Phong** en tiempo real y una interfaz gráfica 2D (**HUD**) para personalización y control de vuelo.

---

## Arquitectura del Software

El proyecto sigue una arquitectura modular estructurada sobre el **Pipeline de Renderizado Programable de OpenGL**, dividida en cuatro capas principales:

1. **Capa de Gestión de Ventana y Entrada (Input):**
   - Implementada con **GLFW**. Captura eventos de teclado (`WASD`, flechas, `Q/E/R`) y eventos de ratón (arrastre 360° e interacción con botones 2D).
2. **Capa Matemática y Lógica de Escena:**
   - Implementada con **GLM**. Gestiona las transformaciones espaciales mediante matrices **MVP** (*Model, View, Projection*), rotación de cámara por ángulos de Euler (*Yaw* y *Pitch*) y evaluación paramétrica de la **curva de Bézier cúbica** $P(t)$ y su vector tangente $P'(t)$.
3. **Capa de Carga de Recursos (Assets):**
   - Implementada con **TinyGLTF** y **stb_image**. Deserializa la geometría binaria del modelo 3D (`Model/Airplane.glb`), extrae sus atributos (vértices, normales, coordenadas UV y textura base) y carga las 6 texturas del entorno cúbico (`Skybox/`).
4. **Capa de Renderizado en GPU (Shaders GLSL):**
   - Tres programas independientes de *Vertex* y *Fragment Shaders*:
     - **Pipeline 3D Principal:** Renderiza el modelo con textura dinámica (tinte de pintura) e iluminación **Phong** (componentes ambiental, difusa y especular).
     - **Pipeline Skybox:** Renderiza el cubo envolvente (`GL_TEXTURE_CUBE_MAP`) con profundidad fija al fondo de la escena.
     - **Pipeline HUD 2D:** Proyección ortográfica (`glm::ortho`) para dibujar los controles interactivos en pantalla.

---

## Estructura de Ramas del Repositorio (Git)

El desarrollo se organizó siguiendo un flujo de trabajo por ramas:
- **`main`:** Rama principal estable con la versión final funcional lista para producción y sustentación.
- **`dev` (o `feature/*`):** Rama de integración y pruebas donde se implementaron de forma incremental el cargador `tinygltf`, la cámara libre 360°, la trayectoria de Bézier y el panel HUD 2D antes de fusionarlos a `main`.

---

## Requisitos y Estructura de Carpetas

Para compilar y ejecutar el proyecto se requiere un compilador C++17 (Visual Studio 2022) con **GLFW**, **GLAD**, **GLM** y **TinyGLTF** configurados, además de la siguiente estructura de archivos en el directorio de ejecución:

```text
├── main.cpp              # Código fuente principal y shaders GLSL
├── README.md             # Documentación técnica del proyecto
├── Model/
│   └── Airplane.glb      # Modelo 3D de la aeronave con textura
└── Skybox/
    ├── right.jpg         # Cara derecha (+X)
    ├── left.jpg          # Cara izquierda (-X)
    ├── top.jpg           # Cara superior (+Y)
    ├── bottom.jpg        # Cara inferior (-Y)
    ├── front.jpg         # Cara frontal (+Z)
    └── back.jpg          # Cara trasera (-Z)
