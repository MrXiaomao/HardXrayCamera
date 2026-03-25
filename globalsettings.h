#ifndef GLOBALSETTINGS_H
#define GLOBALSETTINGS_H

#include <QObject>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>
#include <QReadWriteLock>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QDir>
class JsonSettings : public QObject{
    Q_OBJECT
public:
    JsonSettings(const QString &fileName) {
        QFileInfo mConfigurationFile;
        mConfigurationFile.setFile(fileName);
        mFileName = mConfigurationFile.absoluteFilePath();
        mOpened = this->load();
    };
    ~JsonSettings(){

    };

    bool isOpen() {
        return this->mOpened;
    }

    QString fileName() const{
        return mFileName;
    }

    //添加文件锁
    void prepare(){
        emit sigPrepare(mFileName);
        // mAccessMutex.lock();
    }

    //释放文件锁
    bool finish()
    {
        // mAccessMutex.unlock();
        emit sigFinish(mFileName);
        return mResult;
    }

    Q_DECL_DEPRECATED void beginGroup(const QString &prefix = ""){
        if (prefix.isEmpty()){
            mPrefix = prefix;
            mJsonGroup = QJsonObject();
        } else {
            if (mJsonRoot.contains(prefix)){
                mJsonGroup = mJsonRoot[prefix].toObject();
                mPrefix = prefix;
            } else {
                mJsonGroup = QJsonObject();
                mJsonRoot[prefix] = mJsonGroup;
                mPrefix = prefix;
            }
        }
    };

    Q_DECL_DEPRECATED void endGroup(){
        if (!mPrefix.isEmpty()){
            mJsonRoot[mPrefix] = mJsonGroup;
            mJsonGroup = QJsonObject();
            mPrefix.clear();
        }        
    };

    bool load(){
        QWriteLocker locker(&mRWLock);
        mJsonRoot = QJsonObject();
        mJsonGroup = QJsonObject();
        mPrefix.clear();

        QFile file(mFileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray jsonData = file.readAll();
            file.close();

            QJsonParseError error;
            QJsonDocument mJsonDoc = QJsonDocument::fromJson(jsonData, &error);
            if (error.error == QJsonParseError::NoError) {
                if (mJsonDoc.isObject()) {
                    mJsonRoot = mJsonDoc.object();
                    return true;
                } else {
                    qDebug() << "文件[" << mFileName << "]解析失败！";
                    return false;
                }
            } else{
                qDebug() << "文件[" << mFileName << "]解析失败！" << error.errorString().toUtf8().constData();
                return false;
            }
        } else {
            qDebug() << "文件[" << mFileName << "]打开失败！";

            // 文件不存在时先确保父目录存在，再创建新文件
            QString dirPath = QFileInfo(mFileName).absolutePath();
            if (!dirPath.isEmpty() && !QDir().exists(dirPath))
                QDir().mkpath(dirPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
                file.close();
                return true;
            }
            return false;
        }
    };

    bool save(const QString &fileName = ""){
        QWriteLocker locker(&mRWLock);
        QString path = fileName.isEmpty() ? mFileName : fileName;
        QFile file(path);
        // 保存前确保父目录存在，避免首次写入失败
        QString dirPath = QFileInfo(path).absolutePath();
        if (!dirPath.isEmpty() && !QDir().exists(dirPath))
            QDir().mkpath(dirPath);
        if (file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text)) {
            QJsonDocument jsonDoc(mJsonRoot);
            file.write(jsonDoc.toJson());
            file.close();
            mResult = true;
        } else {
            qDebug() << "文件[" << mFileName << "]信息保存失败！";
            mResult = false;
        }

        return mResult;
    };

    bool flush(){
        return save(mFileName);
    };

    // 辅助函数：解析路径并设置值
    // setValueByPath(settings, "network/ip1", ip_det1);
    void setValueByPath(JsonSettings* settings, const QString& path, const QVariant& value) {
        QStringList parts = path.split('/');
        
        if (parts.size() == 1) {
            settings->setRootValue(parts[0], value);
        } else if (parts.size() == 2) {
            settings->setGroupValue(parts[0], parts[1], value);
        } else if (parts.size() == 3) {
            settings->setGroupValue(parts[0], parts[1], parts[2], value);
        } else {
            qWarning() << "Unsupported path depth:" << path;
        }
    }

    // 推荐使用：对象自身按路径写值
    void setValueByPath(const QString& path, const QVariant& value) {
        setValueByPath(this, path, value);
    }

    // 辅助函数：解析路径并读取值
    QVariant getValueByPath(JsonSettings* settings, const QString& path, const QVariant& defaultValue = QVariant()) {
        QStringList parts = path.split('/');

        if (parts.size() == 1) {
            return settings->rootValue(parts[0], defaultValue);
        } else if (parts.size() == 2) {
            return settings->groupValue(parts[0], parts[1], defaultValue);
        } else if (parts.size() == 3) {
            return settings->groupValue(parts[0], parts[1], parts[2], defaultValue);
        } else {
            qWarning() << "Unsupported path depth:" << path;
            return defaultValue;
        }
    }

    // 推荐使用：对象自身按路径读取值
    QVariant getValueByPath(const QString& path, const QVariant& defaultValue = QVariant()) {
        return getValueByPath(this, path, defaultValue);
    }

    /*
        {
            "键key": "值value",
        }
    */
    void setRootValue(const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        mJsonRoot[key] = value.toJsonValue();
    };

    QVariant rootValue(const QString &key, const QVariant &defaultValue = QVariant())
    {
        QReadLocker locker(&mRWLock);
        if (mJsonRoot.contains(key))
            return mJsonRoot[key].toVariant();
        else
            return defaultValue;
    };

    /*
        {
            "groupName":{
                "键key": "值value",
            }
        }
    */
    void setGroupValue(const QString &groupName, const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end()) {
            QJsonValueRef valueGroupRef = iterator.value();
            QJsonObject objGroup = valueGroupRef.toObject();
            objGroup[key] = value.toJsonValue();
            valueGroupRef = objGroup;
        }
        else {
            QJsonObject objGroup;
            objGroup[key] = value.toJsonValue();
            mJsonRoot.insert(groupName, QJsonValue(objGroup));
        }
    };

    QVariant groupValue(const QString &groupName, const QString &key, const QVariant &defaultValue = QVariant())
    {
        QReadLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end()) {
            QJsonValueRef valueGroupRef = iterator.value();
            QJsonObject objGroup = valueGroupRef.toObject();
            return objGroup[key].toVariant();
        }

        return defaultValue;
    };

    /*
        {
            "groupName":{
                "group2Name":{
                    "键key": "值value",
                }
            }
        }
    */
    void setGroupValue(const QString &groupName, const QString &group2Name, const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueGroupRef = iterator.value();
            if (valueGroupRef.isObject())
            {
                QJsonObject objGroup = valueGroupRef.toObject();
                auto iterator2 = objGroup.find(group2Name);
                if (iterator2 != objGroup.end())
                {
                    QJsonValueRef valueGroupRef2 = iterator2.value();
                    QJsonObject objGroup2 = valueGroupRef2.toObject();
                    objGroup2[key] = value.toJsonValue();
                    valueGroupRef2 = objGroup2;
                }
                else
                {
                    QJsonObject objGroup2;
                    objGroup2[key] = value.toJsonValue();
                    objGroup.insert(group2Name, QJsonValue(objGroup2));
                }

                valueGroupRef = objGroup;
            }
            else
            {
                // 找到字段，但是类型不对
                return;
            }
        }
        else
        {
            QJsonObject objGroup2;
            objGroup2[key] = value.toJsonValue();

            QJsonObject objGroup;
            objGroup.insert(group2Name, QJsonValue(objGroup2));

            mJsonRoot.insert(groupName, QJsonValue(objGroup));
        }
    };

    QVariant groupValue(const QString &groupName, const QString &group2Name, const QString &key, const QVariant &defaultValue = QVariant()){
        QReadLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueGroupRef = iterator.value();
            if (valueGroupRef.isObject())
            {
                QJsonObject objGroup = valueGroupRef.toObject();
                auto iterator2 = objGroup.find(group2Name);
                if (iterator2 != objGroup.end())
                {
                    QJsonValueRef valueGroupRef2 = iterator2.value();
                    QJsonObject objGroup2 = valueGroupRef2.toObject();
                    return objGroup2[key].toVariant();
                }
            }
        }

        return defaultValue;
    };

    /*
        {
            "groupName":{
                "group2Name":{
                    "group3Name":{
                        "键key": "值value",
                    }
                }
            }
        }
    */
    void setGroupValue(const QString &groupName, const QString &group2Name, const QString &group3Name, const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueGroupRef = iterator.value();
            if (valueGroupRef.isObject())
            {
                QJsonObject objGroup = valueGroupRef.toObject();
                auto iterator2 = objGroup.find(group2Name);
                if (iterator2 != objGroup.end())
                {
                    QJsonValueRef valueGroup2Ref = iterator2.value();
                    if (valueGroup2Ref.isObject())
                    {
                        QJsonObject objGroup2 = valueGroup2Ref.toObject();
                        auto iterator3 = objGroup2.find(group3Name);
                        if (iterator3 != objGroup2.end())
                        {
                            QJsonValueRef valueGroupRef3 = iterator3.value();
                            QJsonObject objGroup3 = valueGroupRef3.toObject();
                            objGroup3[key] = value.toJsonValue();

                            objGroup2.insert(group3Name, objGroup3);//valueGroupRef3 = objGroup2;
                        }
                        else
                        {
                            QJsonObject objGroup3;
                            objGroup3[key] = value.toJsonValue();

                            objGroup2.insert(group3Name, objGroup3);
                        }

                        valueGroup2Ref = objGroup2;
                    }
                    else
                    {
                        // 找到字段，但是类型不对
                        return;
                    }
                }
                else
                {
                    QJsonObject objGroup3;
                    objGroup3[key] = value.toJsonValue();

                    QJsonObject objGroup2;
                    objGroup2.insert(group3Name, QJsonValue(objGroup3));

                    objGroup.insert(group2Name, QJsonValue(objGroup2));
                }

                valueGroupRef = objGroup;
            }
            else
            {
                // 找到字段，但是类型不对
                return;
            }
        }
        else
        {
            QJsonObject objGroup3;
            objGroup3[key] = value.toJsonValue();

            QJsonObject objGroup2;
            objGroup2.insert(group3Name, QJsonValue(objGroup3));

            QJsonObject objGroup;
            objGroup.insert(group2Name, QJsonValue(objGroup2));

            mJsonRoot.insert(groupName, QJsonValue(objGroup));
        }
    };

    QVariant groupValue(const QString &groupName, const QString &group2Name, const QString &group3Name, const QString &key, const QVariant &defaultValue = QVariant()){
        QReadLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueGroupRef = iterator.value();
            if (valueGroupRef.isObject())
            {
                QJsonObject objGroup = valueGroupRef.toObject();
                auto iterator2 = objGroup.find(group2Name);
                if (iterator2 != objGroup.end())
                {
                    QJsonValueRef valueGroup2Ref = iterator2.value();
                    if (valueGroup2Ref.isObject())
                    {
                        QJsonObject objGroup2 = valueGroup2Ref.toObject();
                        auto iterator3 = objGroup2.find(group3Name);
                        if (iterator3 != objGroup2.end())
                        {
                            QJsonValueRef valueGroupRef3 = iterator3.value();
                            QJsonObject objGroup3 = valueGroupRef3.toObject();
                            return objGroup3[key].toVariant();
                        }
                    }
                }
            }
        }

        return defaultValue;
    };

    /*
    {
        "arrayName":[
            "键key1": "值value1", //arrayIndex===0
            "键key2": "值value2", //arrayIndex===1
        ]
    }
    */
    void appendArrayValue(const QString &arrayName, const QVariant &value)
    {
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(arrayName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueArrayRef = iterator.value();
            if (valueArrayRef.isArray())
            {
                QJsonArray arrayGroup = valueArrayRef.toArray();
                arrayGroup.append(value.toJsonValue());
                valueArrayRef = arrayGroup;
            }
            else
            {
                // 找到字段，但是类型不对
                return;
            }
        }
        else
        {
            QJsonArray arrayGroup;
            arrayGroup.append(value.toJsonValue());

            mJsonRoot.insert(arrayName, QJsonValue(arrayGroup));
        }
    };

    void setArrayValue(const QString &arrayName, const quint8 &arrayIndex, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(arrayName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueArrayRef = iterator.value();
            if (valueArrayRef.isArray())
            {
                QJsonArray arrayGroup = valueArrayRef.toArray();
                if (arrayIndex < arrayGroup.size())
                {
                    arrayGroup.replace(arrayIndex, value.toJsonValue());
                    valueArrayRef = arrayGroup;
                }
                else
                {
                    // 越界
                    return;
                }
            }
            else
            {
                // 找到字段，但是类型不对
                return;
            }
        }
        else
        {
            // 越界
            return;
        }
    };

    QVariant arrayValue(const QString &arrayName, const quint8 &arrayIndex, const QVariant &defaultValue = QVariant()){
        QReadLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(arrayName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueArrayRef = iterator.value();
            if (valueArrayRef.isArray())
            {
                QJsonArray arrayGroup = valueArrayRef.toArray();
                if (arrayIndex < arrayGroup.size())
                {
                    return arrayGroup.at(arrayIndex).toVariant();
                }
            }
        }

        return defaultValue;
    };

    /*
    {
        "groupName":{
            "arrayName":[
                "键key1": "值value1", //arrayIndex===0
                "键key2": "值value2", //arrayIndex===2
            ]
        }
    }
    */

    /*
    {
        "groupName":{
            "arrayName":[
                {
                    "键key1": "值value1", //arrayIndex===0
                    "键key2": "值value2", //arrayIndex===2
                }
            ]
        }
    }
    */
    void setArrayValue(const QString &groupName, const QString &arrayName, const quint8 &arrayIndex, const QString &key, const QVariant &value){
        QWriteLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueGroupRef = iterator.value();
            if (valueGroupRef.isObject())
            {
                QJsonObject objGroup = valueGroupRef.toObject();
                auto iterator2 = objGroup.find(arrayName);
                if (iterator2 != objGroup.end())
                {
                    QJsonValueRef valueArrayRef = iterator2.value();
                    if (valueArrayRef.isArray())
                    {
                        QJsonArray arrayGroup = valueArrayRef.toArray();
                        if (arrayIndex < arrayGroup.size()){
                            QJsonValueRef valueGroupRef = arrayGroup[arrayIndex];
                            if (valueGroupRef.isObject())
                            {
                                QJsonObject objArray = valueGroupRef.toObject();
                                objArray[key] = value.toJsonValue();

                                valueGroupRef = objArray;
                            }
                            else
                            {
                                // 找到字段，但是类型不对
                                return;
                            }
                        }
                        else
                        {
                            // 数组越界
                            QJsonObject objArray;
                            objArray[key] = value.toJsonValue();

                            arrayGroup.append(objArray);
                        }

                        valueArrayRef = arrayGroup;
                    }
                    else
                    {
                        // 找到字段，但是类型不对
                        return;
                    }
                }
                else
                {
                    QJsonObject objArray;
                    objArray[key] = value.toJsonValue();

                    QJsonArray arrayGroup;
                    arrayGroup.append(objArray);

                    objGroup.insert(arrayName, QJsonValue(arrayGroup));
                }

                valueGroupRef = objGroup;
            }
            else
            {
                // 找到字段，但是类型不对
                return;
            }
        }
        else
        {
            QJsonObject objArray;
            objArray[key] = value.toJsonValue();

            QJsonArray arrayGroup;
            arrayGroup.append(objArray);

            QJsonObject objGroup;
            objGroup.insert(arrayName, QJsonValue(arrayGroup));

            mJsonRoot.insert(groupName, QJsonValue(objGroup));
        }
    };

    QVariant arrayValue(const QString &groupName, const QString &arrayName, const quint8 &arrayIndex, const QString &key, const QVariant &defaultValue = QVariant()){
        QReadLocker locker(&mRWLock);
        auto iterator = mJsonRoot.find(groupName);
        if (iterator != mJsonRoot.end())
        {
            QJsonValueRef valueGroupRef = iterator.value();
            if (valueGroupRef.isObject())
            {
                QJsonObject objGroup = valueGroupRef.toObject();
                auto iterator2 = objGroup.find(arrayName);
                if (iterator2 != objGroup.end())
                {
                    QJsonValueRef valueArrayRef = iterator2.value();
                    if (valueArrayRef.isArray())
                    {
                        QJsonArray arrayGroup = valueArrayRef.toArray();
                        if (arrayIndex < arrayGroup.size()){
                            QJsonValueRef valueGroupRef = arrayGroup[arrayIndex];
                            if (valueGroupRef.isObject())
                            {
                                QJsonObject objArray = valueGroupRef.toObject();
                                return objArray[key].toVariant();
                            }
                        }
                    }
                }
            }
        }

        return defaultValue;
    };

    /*
        适用于跟节点或一级节点赋值
        {
            "键key": "值value",
            "一级节点" :{           //需调用beginGroup进入子节点
                "键key": "值value"
            }
        }
    */
    QT_DEPRECATED_X("Use JsonSettings::setRootValue(QString,QString,QVariant) instead")
    void setValue(const QString &key, const QVariant &value){
        //QWriteLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        // 根据是否在组内判断写入目标：新建组时 mJsonGroup 为空，不能用 isEmpty() 判断
        if (!mPrefix.isEmpty())
            mJsonGroup[key] = QJsonValue::fromVariant(value);
        else
            mJsonRoot[key] = QJsonValue::fromVariant(value);
    };
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant())
    {
        //QReadLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        if (!mPrefix.isEmpty()) {
            if (mJsonGroup.contains(key))
                return mJsonGroup[key].toVariant();
            return defaultValue;
        }
        if (mJsonRoot.contains(key))
            return mJsonRoot[key].toVariant();
        return defaultValue;
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*
        适用于二级节点下数组类型数据访问
        {
            "一级节点" :{            //需调用beginGroup进入子节点
                "二级节点键arrayKey": [
                    {               // index=0
                        "键valueKey" : "值value"
                    },
                    {               // index=1
                        "键valueKey" : "值value"
                    }
                ]
                "二级节点": [
                    {               // index=0
                        "键valueKey" : "值value"
                    }
                ]
            }
        }
    */
    void setArrayValue(const QString &arrayKey, const int &index, const QString &valueKey, const QVariant &value){
        //QWriteLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        QJsonArray jsonArray;
        if (mJsonGroup.contains(arrayKey)){
            jsonArray = mJsonGroup[arrayKey].toArray();
            QJsonObject item;
            if (index >= jsonArray.size()){
                item[valueKey] = QJsonValue::fromVariant(value);
                jsonArray.append(item);
            } else {
                item = jsonArray.at(index).toObject();
                item[valueKey] = QJsonValue::fromVariant(value);
                jsonArray.replace(index, item);
            }
        } else {
            QJsonObject item;
            item[valueKey] = QJsonValue::fromVariant(value);
            jsonArray.append(item);
        }

        mJsonGroup[arrayKey] = jsonArray;
    };
    QVariant arrayValue(const QString &arrayKey, const int &index, const QString &valueKey, const QVariant &defaultValue = QVariant())
    {
        //QReadLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        if (mJsonGroup.contains(arrayKey)){
            QJsonArray jsonArray = mJsonGroup[arrayKey].toArray();
            if (index >= jsonArray.size()){
                return defaultValue;
            } else {
                const QJsonObject mJsonValue = jsonArray[index].toObject();
                return mJsonValue[valueKey].toVariant();
            }
        } else {
            return defaultValue;
        }
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*
        适用于三级节点数据访问
        {
            "一级节点" :{            //需调用beginGroup进入子节点
                "二级节点键subGroup": {
                    "三级节点键childKey" {
                        "键valueKey" : "值value"
                    },
                    "三级节点键childKey" {
                        "键valueKey" : "值value"
                    }
                ]
            }
        }
    */
    void setChildValue(const QString &subGroup, const QString &childKey, const QString &valueKey, const QVariant &value){
        //QWriteLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        QJsonObject jsonSubGroup;
        if (mJsonGroup.contains(subGroup)){
            jsonSubGroup = mJsonGroup[subGroup].toObject();
            QJsonObject jsonChild;
            if (jsonSubGroup.contains(childKey)){
                jsonChild = jsonSubGroup[childKey].toObject();
            }

            jsonChild[valueKey] = QJsonValue::fromVariant(value);
            jsonSubGroup[childKey] = jsonChild;
        }
        else{
            QJsonObject jsonChild;
            jsonChild[valueKey] = QJsonValue::fromVariant(value);
            jsonSubGroup[childKey] = jsonChild;
        }

        mJsonGroup[subGroup] = jsonSubGroup;
    };
    QVariant childValue(const QString &subGroup, const QString &childKey, const QString &valueKey, const QVariant &defaultValue = QVariant())
    {
        //QReadLocker locker(&mRWLock);//beginGroup已经上锁了，这里就不需要了
        QVariant result = defaultValue;
        if (mJsonGroup.contains(subGroup)){
            QJsonObject jsonSubGroup = mJsonGroup[subGroup].toObject();
            if (jsonSubGroup.contains(childKey)){
                QJsonObject jsonChild = jsonSubGroup[childKey].toObject();
                if (jsonChild.contains(valueKey))
                    result = jsonChild[valueKey].toVariant();
                else
                    result = defaultValue;
            } else {
                result = defaultValue;
            }
        }

        return result;
    };

    Q_SIGNAL void sigPrepare(const QString &fileName);
    Q_SIGNAL void sigFinish(const QString &fileName);

protected:
    QString mFileName;
    QString mPrefix;
    QJsonObject mJsonRoot;
    QJsonObject mJsonGroup;
    QReadWriteLock mRWLock;
    QMutex mAccessMutex;//访问锁
    bool mOpened = false;//文档打开成功标识
    bool mResult = false;//保存操作结果
};

class ScopedFileLock {
public:
    explicit ScopedFileLock(JsonSettings* settings) 
        : m_settings(settings) 
    {
        if (m_settings) {
            m_settings->prepare();  // 获取锁并发射 sigPrepare 信号
        }
    }
    
    ~ScopedFileLock() {
        if (m_settings) {
            m_settings->finish();   // 释放锁并发射 sigFinish 信号
        }
    }
    
    // 禁止拷贝
    ScopedFileLock(const ScopedFileLock&) = delete;
    ScopedFileLock& operator=(const ScopedFileLock&) = delete;
    
    // 允许移动
    ScopedFileLock(ScopedFileLock&& other) noexcept 
        : m_settings(other.m_settings) 
    {
        other.m_settings = nullptr;
    }
    
private:
    JsonSettings* m_settings;
};

class GlobalSettings: public QObject
{
    Q_OBJECT
public:
    static GlobalSettings *instance() {
        static GlobalSettings globalSettings;
        return &globalSettings;
    }

    explicit GlobalSettings();
    ~GlobalSettings();

    JsonSettings* mRunSettings;
    JsonSettings* mUserSettings;

private:
    bool mWatchThisFile = true;
    QFileSystemWatcher *mConfigurationFileWatch = nullptr;
};

#endif // GLOBALSETTINGS_H
