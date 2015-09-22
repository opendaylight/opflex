#include <modelgbp/dci/AddressFamilyEnumT.hpp>
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

class MockListener : public opflex::modb::ObjectListener {

  public:

    MockListener(opflex::ofcore::OFFramework& framework)
        : framework_(framework)
    {}

    virtual ~MockListener()
    {}

    virtual void objectUpdated(opflex::modb::class_id_t class_id,
                               const opflex::modb::URI& uri) {

        std::cout << "Object updated: " << uri;

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

            if (obj && (*obj)->isAfSet()) {
                std::cout << " Address Family: "
                          << +*((*obj)->getAf())
                ;
            }
          }
          break;
          case modelgbp::dci::RouteTargetDef::CLASS_ID:
          {
            boost::optional<boost::shared_ptr<modelgbp::dci::RouteTargetDef> >
                obj =
                modelgbp::dci::RouteTargetDef::resolve(framework_, uri);

            std::cout << " Target Address Family: ";
            if (obj && (*obj)->isTargetAfSet()) {
                std::cout << +*((*obj)->getTargetAf());
            } else {
                std::cout << "!!!MISSING!!!";
            }

            std::cout << " Type: ";
            if (obj && (*obj)->isTypeSet()) {
                std::cout << +*((*obj)->getType());
            } else {
                std::cout << "!!!MISSING!!!";
            }

            std::cout << " ASN: ";
            if (obj && (*obj)->isRtASSet()) {
                std::cout << *((*obj)->getRtAS());
            } else {
                std::cout << "!!!MISSING!!!";
            }

            std::cout << " Network #: ";
            if (obj && (*obj)->isRtNNSet()) {
                std::cout << *((*obj)->getRtNN());
            } else {
                std::cout << "!!!MISSING!!!";
            }
          }
          break;
          case modelgbp::dci::DomainToGbpRoutingDomainRSrc::CLASS_ID:
          {
            boost::optional<boost::shared_ptr<modelgbp::dci::DomainToGbpRoutingDomainRSrc> >
                obj =
                modelgbp::dci::DomainToGbpRoutingDomainRSrc::resolve(framework_, uri);

            std::cout << " Target: ";
            if (obj && (*obj)->isTargetSet()) {
                std::cout << "\""
                          << *((*obj)->getTargetURI())
                          << "\""
                ;
            } else {
                std::cout << "!!!MISSING!!!";
            }
          }
          break;
          case modelgbp::gbp::RoutingDomain::CLASS_ID:
          {
            boost::optional<boost::shared_ptr<modelgbp::gbp::RoutingDomain> >
                obj =
                modelgbp::gbp::RoutingDomain::resolve(framework_, uri);

            std::cout << " Global Name: ";
            if (obj && (*obj)->isGlobalNameSet()) {
                std::cout << "\""
                          << *((*obj)->getGlobalName())
                          << "\""
                ;
            } else {
                std::cout << "!!!MISSING!!!";
            }
          }
          break;
        }

        std::cout << std::endl;
    }

  private:
    opflex::ofcore::OFFramework& framework_;
};

int main(int argc, char** argv) {

    opflex::logging::StdOutLogHandler logHandler(opflex::logging::OFLogHandler::INFO);
    opflex::logging::OFLogHandler::registerHandler(logHandler);

    opflex::ofcore::OFFramework framework;

    framework.setModel(modelgbp::getMetadata());
    framework.start();
    framework.addPeer("127.0.0.1", 8009);

    MockListener listener(framework);
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

    pause();
    framework.stop();

    return 0;
}
