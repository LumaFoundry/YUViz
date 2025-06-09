#include "mywindow.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>

MyWindow::MyWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Video Player");
    resize(800, 600);

}

void MyWindow::loadYUV(const QString &filePath, int width, int height) {


}
