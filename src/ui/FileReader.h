#ifndef FILEREADER_H
#define FILEREADER_H

#include <QtGui>

class FileReader : public QPlainTextEdit {
    Q_OBJECT
public:
    virtual ~FileReader() { }
public slots:
    void setFile(QString filePath) {
        QFile textFile(filePath);
        if (textFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream textStream(&textFile);
            clear();
            while (!textStream.atEnd()) {
                appendPlainText(textStream.readLine());
            }
        } else {
            setPlainText("Error reading file.");
        }
    };
};

#endif
