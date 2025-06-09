#include "videoRenderer.h"

VideoRenderer::VideoRenderer(): 
    rhi(nullptr),
    vertexBuffer(nullptr),
    yTex(nullptr),
    uTex(nullptr),
    vTex(nullptr),
    sampler(nullptr),
    bindings(nullptr),
    pipeline(nullptr) {}

VideoRenderer::~VideoRenderer() {

    if (vertexBuffer){
        delete vertexBuffer;
        vertexBuffer = nullptr;
    }

    if (yTex) {
        delete yTex;
        yTex = nullptr;
    }

    if (uTex) {
        delete uTex;
        uTex = nullptr;
    }

    if (vTex) {
        delete vTex;
        vTex = nullptr;
    }

    if (sampler) {
        delete sampler;
        sampler = nullptr;
    }

    if (bindings) {
        delete bindings;
        bindings = nullptr;
    }

    if (pipeline) {
        delete pipeline;
        pipeline = nullptr;
    }

}

void VideoRenderer::uploadFrame(const FrameData& frame) {
    // TODO: safe guard OpenGL context and texture format & size

    bool isCurrentPlanar = frame.isPlanar;
    const QSize YSize = frame.getYSize();
    const QSize CSize = frame.getCSize();

    // Only recreate textures if:
    // 1. They are not allocated yet
    // 2. The frame size (resolution) or format has changed

    if (!yTexture || isCurrentPlanar != frame.isPlanar|| YSize != frame.getYSize()) {
        // Release existing textures
        yTexture.reset();
        uTexture.reset();
        vTexture.reset();

        // Create new textures with updated size and format
        yTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
        yTexture->setSize(YSize.width(), YSize.height());
        yTexture->setFormat(QOpenGLTexture::R8_UNorm);
        yTexture->allocateStorage();

        if (isCurrentPlanar) {
            uTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
            uTexture->setSize(CSize.width(), CSize.height());
            uTexture->setFormat(QOpenGLTexture::R8_UNorm);
            uTexture->allocateStorage();

            vTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
            vTexture->setSize(CSize.width(), CSize.height());
            vTexture->setFormat(QOpenGLTexture::R8_UNorm);
            vTexture->allocateStorage();
        }
    }

    if (!yTexture)
        yTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));

    yTexture->setData(QOpenGLTexture::R8_UNorm, QOpenGLTexture::UInt8, frame.getYData().constData());

    if (frame.isPlanar) {
        if (!uTexture)
            uTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));

        if (!vTexture)
            vTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));

        uTexture->setData(QOpenGLTexture::R8_UNorm, QOpenGLTexture::UInt8, frame.getUData().constData());
        vTexture->setData(QOpenGLTexture::R8_UNorm, QOpenGLTexture::UInt8, frame.getVData().constData());

    } else {
        uTexture.reset();
        vTexture.reset();
    }
}


void VideoRenderer::initialize(QRhi *rhiBackend,
                               const QString &vertexShaderPath,
                               const QString &fragmentShaderPath) {
    if (!rhiBackend) {
        qWarning("RHI backend not initialized");
        return;
    }

    // Load & link shaders
    shaderProgram.addShaderFromSourceFile(QOpenGLShader::Vertex, vertexShaderPath);
    shaderProgram.addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentShaderPath);
    shaderProgram.link();

    if (!shaderProgram.isLinked()) {
        qWarning() << "Shader program failed to link:" << shaderProgram.log();
        return;
    }

    // Draw rectangle vertices
    Vertex vertices[] = {
        {QVector2D(-1.0f, -1.0f), QVector2D(0.0f, 1.0f)}, // top-left
        {QVector2D( 1.0f, -1.0f), QVector2D(1.0f, 1.0f)}, // top-right
        {QVector2D(-1.0f,  1.0f), QVector2D(0.0f, 0.0f)}, // bottom-left
        {QVector2D( 1.0f,  1.0f), QVector2D(1.0f, 0.0f)}  // bottom-right
    };

    // Create vertex buffer object
    vbo.create();
    if (!vbo.isCreated()) {
        qWarning() << "Failed to create VBO";
        return;
    }
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    // Create vertex array object
    vao.create();
    if (!vao.isCreated()) {
        qWarning() << "Failed to create VAO";
        return;
    }
    vao.bind();
    shaderProgram.bind();

    // Bind vertex buffer to shader attributes
    int posLocation = shaderProgram.attributeLocation("aPos");
    if (posLocation == -1) {
        qWarning() << "Attribute 'aPos' not found in shader";
        return;
    }
    glFuncs->glEnableVertexAttribArray(posLocation);
    glFuncs->glVertexAttribPointer(posLocation, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));

    int texCoordLocation = shaderProgram.attributeLocation("aTexCoord");
    if (texCoordLocation == -1) {
        qWarning() << "Attribute 'aTexCoord' not found in shader";
        return;
    }
    glFuncs->glEnableVertexAttribArray(texCoordLocation);
    glFuncs->glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texCoord)));

    // Unbind VAO and VBO
    vao.release();
    vbo.release();
    shaderProgram.release();
}