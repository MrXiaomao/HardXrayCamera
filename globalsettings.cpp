#include "globalsettings.h"
#include <QFileInfo>

/*#########################################################*/
GlobalSettings::GlobalSettings() {
    mRunSettings = new JsonSettings("./config/run.json");
    mUserSettings = new JsonSettings("./config/user.json");
    if (mWatchThisFile){
        // 监视文件内容变化，一旦发现变化重新读取配置文件内容，保持配置信息同步
        mConfigurationFileWatch = new QFileSystemWatcher(this);
        QFileInfo mConfigurationFile;
        mConfigurationFile.setFile(mUserSettings->fileName());
        if (!mConfigurationFileWatch->files().contains(mConfigurationFile.absoluteFilePath()))
            mConfigurationFileWatch->addPath(mConfigurationFile.absoluteFilePath());
        
        connect(mConfigurationFileWatch, &QFileSystemWatcher::fileChanged, this, [=](const QString &fileName){
            if (fileName == mUserSettings->fileName())
                mUserSettings->load();
            else if(fileName == mRunSettings->fileName())
                mRunSettings->load();
        });

        // 链接信号槽，软件自身对配置文件所做的修改，就不用重新读取配置文件了
        std::function<void(const QString &)> onPrepare = [=](const QString &fileName) {
            mConfigurationFileWatch->removePath(fileName);
        };

        
        std::function<void(const QString &)> onFinish = [=](const QString &fileName) {
            if (!mConfigurationFileWatch->files().contains(fileName))
            {
                if (fileName == mUserSettings->fileName()){
                    mUserSettings->load();
                } else if(fileName == mRunSettings->fileName()){
                    mRunSettings->load();
                }
            }
        };

        connect(mRunSettings, &JsonSettings::sigPrepare, this, onPrepare);
        connect(mRunSettings, &JsonSettings::sigFinish, this, onFinish);
        connect(mUserSettings, &JsonSettings::sigPrepare, this, onPrepare);
        connect(mUserSettings, &JsonSettings::sigFinish, this, onFinish);
    }
}

GlobalSettings::~GlobalSettings() {
    // 先断开所有连接
    if (mConfigurationFileWatch) {
        disconnect(mConfigurationFileWatch, nullptr, this, nullptr);
        delete mConfigurationFileWatch;
        mConfigurationFileWatch = nullptr;
    }

        // 断开 JsonSettings 的信号
    if (mRunSettings) {
        disconnect(mRunSettings, nullptr, this, nullptr);
        delete mRunSettings;
        mRunSettings = nullptr;
    }
    
    if (mUserSettings) {
        disconnect(mUserSettings, nullptr, this, nullptr);
        delete mUserSettings;
        mUserSettings = nullptr;
    }
}
