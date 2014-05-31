#ifndef KETUNNELCLIENT_H
#define KETUNNELCLIENT_H

#include "KeMsgProcess.h"

namespace kaerp2p{

class RecorderAvi;
class RecordSaverInterface;

class KeTunnelClient:public KeMsgProcessContainer{
    friend class KeMessageProcessClient;
public:
    KeTunnelClient();
    /**
     * @brief SendCommand
     * @param peer_id
     * @param command --- the message send to another peer
     * @return ---0 : success, 101 : command format error,
     */
    bool SendCommand(const std::string &peer_id,const std::string & command);
    /**
     * @brief StartPeerMedia
     * @param peer_id
     * @param video---1:main stream media,2:sub stream media
     * @return
     */
    virtual bool StartPeerMedia(std::string  peer_id,int video);
    virtual bool StopPeerMedia(std::string peer_id);
    virtual bool StartPeerVideoCut(const std::string &peer_id,
                                   const std::string & filename);
    virtual bool StopPeerVideoCut(const std::string & peer_id);
    //send talk data to camera
    sigslot::signal2<const char *, int > SignalTalkData;
    virtual void SendTalkData(const char * data,int len);
    virtual bool DownloadRemoteFile(std::string  peerId,
                                    std::string remoteFileName,
                                    std::string saveFileName);
    virtual void OnTunnelOpened(PeerTerminalInterface * t,
                                const std::string & peer_id);
    virtual void OnRouterMessage(const std::string &peer_id,
                                 const std::string& msg);
protected:
    virtual void OnRecvAudioData(const std::string & peer_id,
                                 const char * data,int len);
    virtual void OnRecvVideoData(const std::string & peer_id,
                                 const char * data,int len);
    virtual void OnRecordFileData(const std::string & peer_id,
                                  const char * data,int len);
    virtual void OnRecordStatus(const std::string & peer_id,int status);
};


class KeMessageProcessClient: public KeMsgProcess
{
public:
    KeMessageProcessClient(std::string peer_id,KeTunnelClient * container);
    virtual ~KeMessageProcessClient();
    void AskVideo(int video, int listen, int talk);
    bool ReqestPlayFile(const char * remoteFile,const char * saveFile);
    void OnTalkData(const char * data,int len);
    bool StartVideoCut(const std::string &filename);
    bool StopVideoCut();
    sigslot::signal3<const std::string &,const char *,int > SignalRecvVideoData;
    sigslot::signal3<const std::string &,const char *,int > SignalRecvAudioData;
    sigslot::signal3<const std::string &,const char *,int > SignalRecvFileData;
    sigslot::signal2<const std::string &,int> SignalRecordPlayStatus;
protected:
    virtual void OnMessageRespond(talk_base::Buffer & msgData);
    virtual void RecvVideoData(talk_base::Buffer & msgData);
    virtual void RecvAudioData(talk_base::Buffer & msgData);
    virtual void OnRecvRecordMsg(talk_base::Buffer & msgData);
    virtual void RecvAskMediaResp(talk_base::Buffer & msgData);
    virtual void RecvPlayFileResp(talk_base::Buffer & msgData);
private:
    //RecorderAvi *cutter_;
    RecordSaverInterface *recordSaver;
    std::string requestReocrdFileName;
};

}
#endif // KETUNNELCLIENT_H
