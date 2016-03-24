#include <iostream>
#include <modelgbp/dci/AddressFamilyEnumT.hpp>
#include <modelgbp/dci/EpToUnivRSrc.hpp>
#include <modelgbp/dci/RouteTargetDef.hpp>
#include <modelgbp/dci/RouteTargetPdef.hpp>
#include <modelgbp/dci/TargetTypeEnumT.hpp>
#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/gbp/RoutingDomain.hpp>
#include <modelgbp/metadata/metadata.hpp>
#include <opflex/logging/OFLogHandler.h>
#include <opflex/logging/StdOutLogHandler.h>
#include <opflex/modb/Mutator.h>
#include <opflex/modb/ObjectListener.h>
#include <opflex/modb/PropertyInfo.h>
#include <opflex/modb/mo-internal/StoreClient.h>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/ofcore/PeerStatusListener.h>
#include <sstream>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctime>
#include <cstddef>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>

std::string getNewFile(char const * ext);

size_t eventCounter = 0;
opflex::ofcore::OFFramework framework0;
opflex::ofcore::OFFramework framework1;
opflex::ofcore::OFFramework framework2;
opflex::ofcore::OFFramework framework3;
#define MOCK_CLIENT_DEBUG_INTO_A_FILE

namespace opflex { namespace logging { namespace internal {
/* bogus */
class Logger {
  public:
    std::ostream & stream() __attribute__((no_instrument_function));

    Logger(opflex::logging::OFLogHandler::Level const level,
           char const * file,
           int const line,
           char const * function) __attribute__((no_instrument_function));

    ~Logger() __attribute__((no_instrument_function));
private:
    OFLogHandler::Level const level_;
    char const * file_;
    int const line_;
    char const * function_;

    std::ostringstream buffer_;
};
}

class FileStdOutLogHandler : public OFLogHandler {
public:
    /**
     * Allocate a log handler that will log any messages with equal or
     * greater severity than the specified log level.
     * 
     * @param logLevel the minimum log level
     */
    FileStdOutLogHandler(Level logLevel)
        __attribute__((no_instrument_function));
    virtual ~FileStdOutLogHandler()
        __attribute__((no_instrument_function));

    /* see OFLogHandler */
    virtual void handleMessage(const std::string& file,
                               const int line,
                               const std::string& function,
                               const Level level,
                               const std::string& message)
        __attribute__((no_instrument_function));
private:
    boost::iostreams::stream<boost::iostreams::file_sink> fstr;
};

FileStdOutLogHandler::FileStdOutLogHandler(Level logLevel_): OFLogHandler(logLevel_), fstr(getNewFile("log")) { }
FileStdOutLogHandler::~FileStdOutLogHandler() { }
void FileStdOutLogHandler::handleMessage(const std::string& file,
                                     const int line,
                                     const std::string& function,
                                     const Level level,
                                     const std::string& message) {
    if (level < logLevel_) return;

    std::cout << file << ":" << line << ":" << function <<
        "[" << level <<"] " << message << std::endl;
    fstr << file << ":" << line << ":" << function <<
        "[" << level <<"] " << message << std::endl;
}

}}

#define LOG(lvl) \
        opflex::logging::internal::Logger(opflex::logging::OFLogHandler::lvl, __FILE__, __LINE__, __PRETTY_FUNCTION__).stream() << "#" << eventCounter << "#"

#ifdef MOCK_CLIENT_DEBUG_INTO_A_FILE
#define MAX_FILE_NAME_LEN 260
FILE *mock_client_dbg_log_file=NULL;
std::time_t t = std::time(NULL);
struct stat dirstats;
int next=0;

char debug_file[MAX_FILE_NAME_LEN];

#define MOCK_CLIENT_SECRET_FOLDER ".mock_client"
std::string getNewFile(char const * ext) {
    if (!(eventCounter % 1000))
    {
        snprintf(debug_file,MAX_FILE_NAME_LEN, MOCK_CLIENT_SECRET_FOLDER "/%06d_%012d_%05d/%03d", next, t, getpid(), eventCounter/1000);
        mkdir(debug_file, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    }
    snprintf(debug_file,MAX_FILE_NAME_LEN, MOCK_CLIENT_SECRET_FOLDER "/%06d_%012d_%05d/%03d/mock_client.%05d.%05d.%s", next, t, getpid(), eventCounter/1000, getpid(), eventCounter, ext);
    ++eventCounter;
    return std::string(debug_file);
#if 0
    mock_client_dbg_log_file = fopen(debug_file,"w+"); 
    if(mock_client_dbg_log_file == NULL) {
        LOG(INFO) << "unable to open dump file";
        mock_client_dbg_log_file = fopen("/dev/null", "w+");
    }
    return mock_client_dbg_log_file;
#endif
}
#endif

void dump(int s = 0){
    framework0.dumpMODB(getNewFile("f0.dump"));
    framework1.dumpMODB(getNewFile("f1.dump"));
    framework2.dumpMODB(getNewFile("f2.dump"));
    framework3.dumpMODB(getNewFile("f3.dump"));
    if (s) {
        LOG(INFO) << "caught signal " << s << " done";
        framework0.stop();
        framework1.stop();
        framework2.stop();
        framework3.stop();
        exit(0);
    }
}


class MockListener : public opflex::modb::ObjectListener {

  public:

    MockListener(opflex::ofcore::OFFramework& framework, char const * extension)
        : framework_(framework), extension_(extension)
    {}

    virtual ~MockListener()
    {}

    virtual void objectUpdated(opflex::modb::class_id_t class_id,
                               const opflex::modb::URI& uri) {

        std::stringstream oneLiner;

        oneLiner << "Object updated: " << uri;

        switch (class_id) {
          case modelgbp::dci::Universe::CLASS_ID:
          {
            boost::optional<boost::shared_ptr<modelgbp::dci::Universe> >
                obj =
                modelgbp::dci::Universe::resolve(framework_, uri);
          }
          break;
          case modelgbp::dci::Domain::CLASS_ID:
          {
            boost::optional<boost::shared_ptr<modelgbp::dci::Domain> >
                obj =
                modelgbp::dci::Domain::resolve(framework_, uri);
          }
          break;
          case modelgbp::dci::RouteTargetPdef::CLASS_ID:
          {
            boost::optional<boost::shared_ptr<modelgbp::dci::RouteTargetPdef> >
                obj =
                modelgbp::dci::RouteTargetPdef::resolve(framework_, uri);

            oneLiner << " Address Family: ";
            if (obj && (*obj)->isAfSet()) {
                oneLiner << +*((*obj)->getAf());
            } else {
                oneLiner << "!!!MISSING!!!";
            }
          }
          break;
          case modelgbp::dci::RouteTargetDef::CLASS_ID:
          {
            boost::optional<boost::shared_ptr<modelgbp::dci::RouteTargetDef> >
                obj =
                modelgbp::dci::RouteTargetDef::resolve(framework_, uri);

            oneLiner << " Target Address Family: ";
            if (obj && (*obj)->isTargetAfSet()) {
                oneLiner << +*((*obj)->getTargetAf());
            } else {
                oneLiner << "!!!MISSING!!!";
            }

            oneLiner << " Type: ";
            if (obj && (*obj)->isTypeSet()) {
                oneLiner << +*((*obj)->getType());
            } else {
                oneLiner << "!!!MISSING!!!";
            }

            oneLiner << " ASN: ";
            if (obj && (*obj)->isRtASSet()) {
                oneLiner << *((*obj)->getRtAS());
            } else {
                oneLiner << "!!!MISSING!!!";
            }

            oneLiner << " Network #: ";
            if (obj && (*obj)->isRtNNSet()) {
                oneLiner << *((*obj)->getRtNN());
            } else {
                oneLiner << "!!!MISSING!!!";
            }
          }
          break;
          case modelgbp::dci::DomainToGbpRoutingDomainRSrc::CLASS_ID:
          {
            boost::optional<boost::shared_ptr<modelgbp::dci::DomainToGbpRoutingDomainRSrc> >
                obj =
                modelgbp::dci::DomainToGbpRoutingDomainRSrc::resolve(framework_, uri);

            oneLiner << " Target: ";
            if (obj && (*obj)->isTargetSet()) {
                oneLiner << "\""
                          << *((*obj)->getTargetURI())
                          << "\""
                ;
            } else {
                oneLiner << "!!!MISSING!!!";
            }
          }
          break;
          case modelgbp::gbp::RoutingDomain::CLASS_ID:
          {
            boost::optional<boost::shared_ptr<modelgbp::gbp::RoutingDomain> >
                obj =
                modelgbp::gbp::RoutingDomain::resolve(framework_, uri);

            oneLiner << " Global Name: ";
            if (obj && (*obj)->isGlobalNameSet()) {
                oneLiner << "\""
                          << *((*obj)->getGlobalName())
                          << "\""
                ;
            } else {
                oneLiner << "!!!MISSING!!!";
            }
          }
          break;
        }

        dump();
        LOG(INFO) << oneLiner.str();
    }

  private:
    opflex::ofcore::OFFramework& framework_;
    char const * const extension_;
};

void setupFramework(opflex::ofcore::OFFramework& framework, char const * const domain, char const * const peer, char const * const extension) {
    framework.setModel(modelgbp::getMetadata());
    framework.start();
    framework.setOpflexIdentity(
             /* NAME */         "mockclient",
            /* DOMAIN */        domain
                                );
    framework.addPeer(peer, 8009);
    framework.enableSSL("/tmp", false);

    MockListener listener(framework, extension);
    modelgbp::gbp::RoutingDomain::registerListener(framework, &listener);
    modelgbp::dci::Universe::registerListener(framework, &listener);
    modelgbp::dci::Domain::registerListener(framework, &listener);
    modelgbp::dci::DomainToGbpRoutingDomainRSrc::registerListener(framework, &listener);
    modelgbp::dci::RouteTargetPdef::registerListener(framework, &listener);
    modelgbp::dci::RouteTargetDef::registerListener(framework, &listener);
    modelgbp::gbp::RoutingDomain::registerListener(framework, &listener);

    opflex::modb::Mutator mutator(framework, "init");
    boost::shared_ptr<modelgbp::dmtree::Root> root =
        modelgbp::dmtree::Root::createRootElement(framework);
    root->addDciDiscoverer()
        ->addDciEp()
        ->addDciEpToUnivRSrc()
        ->setTargetUniverse();
    mutator.commit();
}

int main(int argc, char** argv) {

#ifdef MOCK_CLIENT_DEBUG_INTO_A_FILE
    {   /* let's nest into a separate block and... */
        if( !stat(MOCK_CLIENT_SECRET_FOLDER, &dirstats)){
//            chdir(MOCK_CLIENT_SECRET_FOLDER);
            next=dirstats.st_nlink-1; /* there will be 2 hardlinks when empty, and we start counting from 1 */

            /* create directory for this instance */
            snprintf(debug_file,MAX_FILE_NAME_LEN, MOCK_CLIENT_SECRET_FOLDER "/%06d_%012d_%05d", next, t, getpid());
#undef MOCK_CLIENT_SECRET_FOLDER
            mkdir(debug_file, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH); 
        }
    }   /* ...free some hundreds bytes from the stack */
#endif
    opflex::logging::FileStdOutLogHandler logHandler(opflex::logging::OFLogHandler::DEBUG2);
    opflex::logging::OFLogHandler::registerHandler(logHandler);

    char const * peer = argv[1] ? : "172.23.137.13";
    setupFramework(framework0, "dci-[50.3.50.1]", peer, "f0.dump");
    setupFramework(framework1, "dci-[50.3.51.1]", peer, "f1.dump");
    setupFramework(framework2, "dci-[50.3.52.1]", peer, "f2.dump");
    setupFramework(framework3, "dci-[50.3.53.1]", peer, "f3.dump");

    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = dump;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    pause();

    return 0;
}
