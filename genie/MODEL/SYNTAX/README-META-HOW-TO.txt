#
# GUIDELINES ON HOW TO SPECIFY PARSE METADATA
#
metadata
{
    .
    .
    .
    # specifies parse node
    # with parse node class corresponding to the name of the node
    # it will be in the p[name] package in the P[Name]Node class
    node[name]
    {
        # specifies qualifier
        qual[name]

        # specifies property name
        prop[name]

        # specifies parse node
        # with parse node class corresponding to the name of the node
        # it will be in the p[package-name-suffix] package in the P[Name]Node class
        # namespace=... :optionally specifies in which namespace/package corresponding
        #                parsing behavior is if auto non-generic is used
        node[name;
             namespace=package-name-suffix
             ]
        {
            .
            .
            .
        }

        # specification of the node that uses other nodes parser and properties
        # use=... :directs to use other node parsing rule
        node[name;
             use=node-name/node-name/...]

        # specification of the node that uses other nodes parser and properties
        # but does not inherit node properties (inherit-props is defaulted to yes)
        node[name;
             use=node-name/node-name/...;
             inherit-props=no]
        {
            .
            .
            .
        }

        # specification of a node that directs to use specific java class for parsing
        node[name
             parser=fully-qualified-java-class-name]
        {

        }

        # specification of the node that is just a placeholder. it doesn't result in
        # anything that produces model items, and, therefore, has no functional behaviors.
        # this is used for nodes that do not produce data or just as a placeholder until
        # functionality is implemented.
        # generic: identifies that this parsing node has no functional behaviors and doesn't
        #          result in model items
        node[name;
             generic]
        {
            .
            .
            .
        }



    }

    # specifies reference to already specified parsing node
    ref[name]
    {
        ref[name]
        {
            node[name]
            .
            .
            .
        }

        node[name]
        {
            node[name]
            .
            .
            .
        }
    }
}