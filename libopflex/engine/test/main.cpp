
/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#define BOOST_TEST_MODULE "Engine"
#include <boost/test/unit_test.hpp>

#include "opflex/logging/StdOutLogHandler.h"

using namespace opflex::logging;

class EngineTest {
public:
    EngineTest() {
        OFLogHandler::registerHandler(testLogger);
    }

    static StdOutLogHandler testLogger;
};

StdOutLogHandler EngineTest::testLogger(OFLogHandler::DEBUG2);

BOOST_GLOBAL_FIXTURE(EngineTest);
