#include <QtGui>
#include "FileReader.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QWidget window;

    QLabel label(QApplication::translate("scriptfile", "Select File:"));
    QLineEdit lineEdit;

    QPushButton browseButton(QApplication::translate("browse", "Browse..."), &window);
    QFileDialog browser;
    QObject::connect(&browseButton, SIGNAL(clicked()), &browser, SLOT(exec()));
    QObject::connect(&browser, SIGNAL(fileSelected(QString)), &lineEdit, SLOT(setText(QString)));
    browser.setFileMode(QFileDialog::ExistingFile);
    browser.setNameFilter("*.txt");

    QVBoxLayout layout;
    QHBoxLayout fileSelectRow;
    fileSelectRow.addWidget(&label);
    fileSelectRow.addWidget(&lineEdit);
    fileSelectRow.addWidget(&browseButton);
    layout.addLayout(&fileSelectRow);

    QStringList colNames;
    colNames << "type" << "mode" << "input/command" << "display";
    int colcount = colNames.length();
    QTableWidget table(0,colcount);
    table.setHorizontalHeaderLabels(colNames);
    QHeaderView* header = table.horizontalHeader();
    header->setStretchLastSection(true);
    for (int c = 0; c < colcount; c++) {
        header->setResizeMode(c,QHeaderView::ResizeToContents);
    }
    layout.addWidget(&table);
    
    QScrollArea scroll;
    FileReader editor;
    editor.setReadOnly(true);
    QObject::connect(&browser, SIGNAL(fileSelected(QString)), &editor, SLOT(setFile(QString)));
    scroll.setWidget(&editor);
    scroll.setWidgetResizable(true);
    layout.addWidget(&scroll);

    window.setLayout(&layout);
    window.setWindowTitle(QApplication::translate("toplevel","File Viewer"));
    window.show();

    return app.exec();
}
