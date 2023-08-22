import sys

from PyQt5.QtWidgets import (QPushButton, QWidget,
                             QLineEdit, QApplication)


class Button(QPushButton):

    def __init__(self, title, parent):
        super().__init__(title, parent)
        self.root = parent
        self.setAcceptDrops(True)

    def dragEnterEvent(self, e):
        print(e)
        print(e.mimeData().data('text/uri-list'))

        if e.mimeData().hasFormat('text/uri-list'):
            e.accept()
        else:
            e.ignore()

    def dropEvent(self, e):

        self.setText(e.mimeData().text())
        self.root.edit.insert(e.mimeData().text())

class Example(QWidget):

    def __init__(self):
        super().__init__()

        self.initUI()

    def initUI(self):

        self.edit = QLineEdit('', self)
        self.edit.setPlaceholderText("THis should be a filename") 
        self.edit.setDragEnabled(True)
        self.edit.setMinimumWidth(100)
        self.edit.move(30, 65)

        button = Button("Button", self)
        button.move(190, 65)

        self.setWindowTitle('Simple drag and drop')
        self.setGeometry(300, 300, 300, 150)


def main():

    app = QApplication(sys.argv)
    ex = Example()
    ex.show()
    app.exec_()


if __name__ == '__main__':
    main()