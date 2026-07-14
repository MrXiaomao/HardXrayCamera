#ifndef HDADATAUPLOAD_H
#define HDADATAUPLOAD_H

#include <QObject>

class HDADataUpload : public QObject
{
    Q_OBJECT
public:
    explicit HDADataUpload(QObject *parent = nullptr);

signals:
};

#endif // HDADATAUPLOAD_H
