/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _OPFLEX_ENFORCER_INTERNAL_MODB_H_
#define _OPFLEX_ENFORCER_INTERNAL_MODB_H_

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include <opflex/modb/URI.h>

/*
 * TODO TEMPORARY HACK - these should come from MODB/Framework headers
 */
namespace opflex {
namespace modb {

typedef unsigned long class_id_t;

class ObjectListener {
public:
    virtual ~ObjectListener() = 0;
    virtual void objectUpdated(class_id_t class_id, const URI& uri) = 0;
};

void RegisterListener(class_id_t, ObjectListener&);
void UnregisterListener(class_id_t, ObjectListener&);

class MoBase {
public:
    MoBase(class_id_t cid, const URI& u) : classId(cid), uri(u) {}

    class_id_t GetClass() const { return classId; }
    const URI& GetUri() const { return uri; }

    std::string Get(const std::string& propName) const {
        if (props.find(propName) != props.end()) {
            return props.find(propName)->second;
        }
        return "";
    }

    void Set(const std::string& propName, const std::string& propValue) {
        props[propName] = propValue;
    }

private:
    class_id_t classId;
    URI uri;
    boost::unordered_map<std::string, std::string> props;
};

typedef boost::shared_ptr<MoBase>       MoBasePtr;
typedef boost::shared_ptr<const MoBase> MoBaseConstPtr;

MoBasePtr Find(const URI& uri);
bool Add(MoBasePtr newObj);
bool Remove(const URI& uri);

}   // namespace modb
}   // namespace opflex


/* END HACK */



#endif //_OPFLEX_ENFORCER_INTERNAL_MODB_H_
