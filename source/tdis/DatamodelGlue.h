
// Copyright 2023, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.


#ifndef JANA2_DATAMODELGLUE_H
#define JANA2_DATAMODELGLUE_H

#include <string>

//#include "datamodel/ExampleHitCollection.h"
//#include "datamodel/ExampleClusterCollection.h"
//#include "datamodel/EventInfoCollection.h"
//#include "datamodel/TimesliceInfoCollection.h"
//
//
template<typename ... Ts>
struct Overload : Ts ... {
    using Ts::operator() ...;
};
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;


template <typename F, typename... ArgsT>
void visitPodioType(const std::string& podio_typename, F& helper, ArgsT... args) {

//    if (podio_typename == "EventInfo") {
//        return helper.template operator()<EventInfo>(std::forward<ArgsT>(args)...);
//    }
//    if (podio_typename == "TimesliceInfo") {
//        return helper.template operator()<TimesliceInfo>(std::forward<ArgsT>(args)...);
//    }
//    else if (podio_typename == "ExampleHit") {
//        return helper.template operator()<ExampleHit>(std::forward<ArgsT>(args)...);
//    }
//    else if (podio_typename == "ExampleCluster") {
//        return helper.template operator()<ExampleCluster>(std::forward<ArgsT>(args)...);
//    }
//    throw std::runtime_error("Not a podio typename!");
}

// If you are using C++20, you can use templated lambdas to write your visitor completely inline like so:




template <typename Visitor>
struct VisitExampleDatamodel {
    void operator()(Visitor& visitor, const podio::CollectionBase &collection) {
        const auto podio_typename = collection.getTypeName();
        if (podio_typename == "EventInfoCollection") {
            return visitor(static_cast<const EventInfoCollection &>(collection));
        } else if (podio_typename == "TimesliceInfoCollection") {
            return visitor(static_cast<const TimesliceInfoCollection &>(collection));
        } else if (podio_typename == "ExampleHitCollection") {
            return visitor(static_cast<const ExampleHitCollection &>(collection));
        } else if (podio_typename == "ExampleClusterCollection") {
            return visitor(static_cast<const ExampleClusterCollection &>(collection));
        }
        throw std::runtime_error("Unrecognized podio typename!");
    }
};


#endif //JANA2_DATAMODELGLUE_H
