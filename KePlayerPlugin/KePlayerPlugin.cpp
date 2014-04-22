#include "KePlayerPlugin.h"

#include <vector>

#include <QtWidgets>
#include <QFile>
#include "VideoWall.h"
#include "talk/base/json.h"
KePlayerPlugin::KePlayerPlugin(QWidget *parent)
    : QWidget(parent),
      connection_(new   PeerConnectionClientDealer()),
      tunnel_(new KeQtTunnelClient()),
      is_inited(false)
{
    QVBoxLayout *vbox = new QVBoxLayout( this );
    vbox->setMargin(0);
    video_wall_ = new VideoWall(this);
    vbox->addWidget( video_wall_ );
    m_savePath = QDir::currentPath();

    QObject::connect(video_wall_,&VideoWall::SigNeedStopPeerPlay,
                     this,&KePlayerPlugin::StopVideo);
    QObject::connect(this,&KePlayerPlugin::TunnelClosed,
                     video_wall_,&VideoWall::StopPeerPlay);

}

KePlayerPlugin::~KePlayerPlugin()
{

}

void KePlayerPlugin::about()
{
    QMessageBox::aboutQt(this);
}

void KePlayerPlugin::SetDivision(int num)
{
    this->video_wall_->SetDivision(num);
}

int KePlayerPlugin::PlayLocalFile()
{
    QString filename = QFileDialog::getOpenFileName(
                this,
                "Open Video File",
                QDir::currentPath(),
                "Video files (*.h264 *.264);;All files(*.*)");
    if (!filename.isNull()) { //用户选择了文件
        qDebug()<<QDir::currentPath();
        this->video_wall_->PlayLocalFile("",filename,0);
    }
    return 0;
}

int KePlayerPlugin::Initialize(QString routerUrl)
{
    if(is_inited){
        return 10002;
    }
    //int ret = connection_->Connect("tcp://192.168.40.191:5555","");
    int ret = connection_->Connect(routerUrl.toStdString(),"");
    if(ret != 0){
        return ret;
    }
    tunnel_->Init(connection_.get());
    QObject::connect(tunnel_.get(),&KeQtTunnelClient::SigRecvVideoData,
                     this->video_wall_,&VideoWall::OnRecvMediaData);
    QObject::connect(tunnel_.get(),&KeQtTunnelClient::SigRecvAudioData,
                     this->video_wall_,&VideoWall::OnRecvMediaData);
    QObject::connect(tunnel_.get(),&KeQtTunnelClient::SigTunnelOpened,
                     this,&KePlayerPlugin::TunnelOpened);
    QObject::connect(tunnel_.get(),&KeQtTunnelClient::SigTunnelClosed,
                     this,&KePlayerPlugin::TunnelClosed);
    QObject::connect(tunnel_.get(),&KeQtTunnelClient::SigRecordStatus,
                     this,&KePlayerPlugin::RecordStatus);
    QObject::connect(tunnel_.get(),&KeQtTunnelClient::SigRecvPeerMsg,
                     this,&KePlayerPlugin::RecvPeerMsg);




    this->is_inited = true;
    return 0;
}

int KePlayerPlugin::StartVideo(QString peer_id)
{
    std::string str_id = peer_id.toStdString();
    int ret =  tunnel_->StartPeerMedia(str_id,true);
    if(ret != 0){
        return ret;
    }

    int index = video_wall_->SetPeerPlay(peer_id);
    qDebug()<<"KePlayerPlugin::StartVideo play index is "<< index;


}

int KePlayerPlugin::StopVideo(QString peer_id)
{
    if(peer_id.isEmpty()){
        return 10001;
    }
    video_wall_->StopPeerPlay(peer_id);
    std::string str_id = peer_id.toStdString();
    return tunnel_->StartPeerMedia(str_id,false);
}

int KePlayerPlugin::SendCommand(QString peer_id, QString msg)
{
    std::string str_id = peer_id.toStdString();
    std::string str_msg = msg.toStdString();
    return tunnel_->SendCommand(str_id,str_msg);
}

int KePlayerPlugin::PlayRecordFiles(QString peer_id, QString record_info_list)
{
    std::string str_id = peer_id.toStdString();

    Json::Reader reader;
    Json::Value jmessage;
    if (!reader.parse(record_info_list.toStdString(), jmessage)) {
        qWarning() << "Received unknown message. " << record_info_list;
        return 10001;
    }
    std::vector<Json::Value> record_vector;
    if(!JsonArrayToValueVector(jmessage,&record_vector)){
        qWarning() << "parse  record_info_list error. " << record_info_list;
        return 10001;
    }
    need_play_records_.clear();
    std::vector<Json::Value>::iterator it = record_vector.begin();
    for(;it != record_vector.end();++it){
        RecordFileInfo file_info;
        file_info.peer_id = str_id;
        if(!GetIntFromJsonObject(*it,"file_size",&file_info.size)){
            qWarning() << "get file size error. " << record_info_list;
            return 10003;
        }
        if(!GetStringFromJsonObject(*it,"file_name",&file_info.remote_name)){
            qWarning() << "get file name error. " <<record_info_list;
            return 10003;
        }
        if(!GetStringFromJsonObject(*it,"file_date",&file_info.file_date)){
            qWarning() << "get file date error. " << record_info_list;
            return 10003;
        }
        need_play_records_.enqueue(file_info);
    }
    if(need_play_records_.empty()){

        return 10002;
    }
    RecordFileInfo first_record = need_play_records_.dequeue();
    bool result = tunnel_->DownloadRemoteFile(str_id,first_record.remote_name);
    if(!result){
        qWarning() << "tunnel DownloadRemoteFile error ";
        return 10005;
    }

    KeRecorder * recorder = new KeRecorder(this);
    recorder->setObjectName(peer_id);
    QObject::connect(recorder,&KeRecorder::SigAbleToPlay,
                     this->video_wall_,&VideoWall::PlayLocalFile);
    QObject::connect(this,&KePlayerPlugin::RemoteFileDownloadEnd,
                     recorder,&KeRecorder::OnRecordDownloadEnd);


    recorder->OpenFile(first_record,m_savePath);

    return 0;
}

int KePlayerPlugin::StopPlayFile(QString peer_id)
{
    KeRecorder * recorder = this->findChild<KeRecorder *>(peer_id);
    if(recorder){
        delete recorder;
    }
    this->video_wall_->StopFilePlay(peer_id);

}

void KePlayerPlugin::setSavePath(const QString &path)
{
    this->m_savePath = path;
}

void KePlayerPlugin::OnRecordStatus(QString peer_id, int status)
{
    std::string str_id = peer_id.toStdString();
    if(status == 6){//download end , download another file
        if(!need_play_records_.empty()){
            RecordFileInfo file = need_play_records_.dequeue();
            ASSERT(str_id == file.peer_id);
            tunnel_->DownloadRemoteFile(str_id,need_play_records_.at(0).remote_name);

            KeRecorder * recorder = this->findChild<KeRecorder *>(peer_id);
            if(recorder){
                this->video_wall_->SetLocalPlayFileSize(peer_id,recorder->GetFileSize());
            }

        }else{ //download end
            emit RemoteFileDownloadEnd(peer_id);
        }
    }
}

QString KePlayerPlugin::savePath() const
{
    return m_savePath;
}


int KePlayerPlugin::OpenTunnel(QString peer_id)
{
    std::string str_id = peer_id.toStdString();
    int ret = tunnel_->OpenTunnel(str_id);
    return ret;
}

int KePlayerPlugin::CloseTunnel(QString peer_id)
{
    std::string str_id = peer_id.toStdString();
    int ret = tunnel_->CloseTunnel(str_id);
    return ret;
}
