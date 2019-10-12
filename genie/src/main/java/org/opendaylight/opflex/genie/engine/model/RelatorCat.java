package org.opendaylight.opflex.genie.engine.model;

import java.util.Map;
import java.util.TreeMap;

import org.opendaylight.opflex.modlan.report.Severity;

/**
 * Created by midvorki on 3/27/14.
 */
public class RelatorCat extends Cat
{
    public enum Type
    {
        FROM,
        TO
    }

    public enum Direction
    {
        DIRECT,
        INVERSE
    }

    public Relator getRelator(String aInName)
    {
        return (Relator) getItem(aInName);
    }

    public Relator getInverseRelator(String aInName)
    {
        RelatorCat lRelCat = RelatorCat.getCreateFromInverse(
                this.getName(),
                Cardinality.MULTI, // YES, INVERSE IS ALWAYS MULTI this.getCardinality(),
                false);

        return null == lRelCat ? null : lRelCat.getRelator(aInName);
    }

    public Cardinality getCardinality()
    {
        return cardinality;
    }

    public Type getType()
    {
        return type;
    }

    public Direction getDirection()
    {
        return direction;
    }

    public static RelatorCat getCreate(String aIn, Cardinality aInCardinality)
    {
        return getCreate(aIn, aInCardinality, Type.FROM, Direction.DIRECT, true);
    }

    public static RelatorCat getCreateToDirect(String aIn, Cardinality aInCardinality, boolean aInCreateIfNotFound)
    {
        return RelatorCat.getCreate(
                aIn + "-TO",
                aInCardinality,
                RelatorCat.Type.TO,
                RelatorCat.Direction.DIRECT,
                aInCreateIfNotFound);
    }

    public static RelatorCat getCreateFromInverse(String aIn, Cardinality aInCardinality, boolean aInCreateIfNotFound)
    {
        return RelatorCat.getCreate(
                "INV-" + aIn,
                aInCardinality,
                RelatorCat.Type.FROM,
                Direction.INVERSE,
                aInCreateIfNotFound);
    }

    public static RelatorCat getCreateToInverse(String aIn, Cardinality aInCardinality, boolean aInCreateIfNotFound)
    {
        return RelatorCat.getCreate(
                "INV-" + aIn + "-TO",
                aInCardinality,
                RelatorCat.Type.TO,
                Direction.INVERSE,
                aInCreateIfNotFound);
    }

    private static synchronized RelatorCat getCreate(
            String aIn, Cardinality aInCardinality, Type aInType, Direction aInDir, boolean aInCreateIfNotFound)
    {
        RelatorCat lCat = (RelatorCat) get(aIn);
        if (null == lCat && aInCreateIfNotFound)
        {
            lCat = new RelatorCat(aIn, aInCardinality, aInType, aInDir);
        }
        return lCat;
    }

    public Relator add(
            Item aInFromItem,
            Item aInToItem
                      )
    {
        return add(
                aInFromItem.getNode().getCat(),
                aInFromItem.getNode().getGID().getName(),
                aInToItem.getNode().getCat(),
                aInToItem.getNode().getGID().getName()
                  );
    }

    public Relator add(
            Item aInFromItem,
            Cat aInToCat,
            String aInToGName)
    {
        return add(
                aInFromItem.getNode().getCat(),
                aInFromItem.getNode().getGID().getName(),
                aInToCat,
                aInToGName);
    }

    public Relator add(
            Node aInFromNode,
            Node aInToNode)
    {
        return add(
                aInFromNode.getCat(),
                aInFromNode.getGID().getName(),
                aInToNode.getCat(),
                aInToNode.getGID().getName());
    }

    public Relator add(
            Node aInFromNode,
            Cat aInToCat,
            String aInToGName)
    {
        return add(
                aInFromNode.getCat(),
                aInFromNode.getGID().getName(),
                aInToCat,
                aInToGName);
    }

    public Relator add(
            Cat aInFromCat,
            String aInFromGName,
            Cat aInToCat,
            String aInToGName)
    {
        //System.out.println("\n\n### Relator.add(" + aInFromCat + "," + aInFromGName + ", " + aInToCat + ", "  + aInToGName + ")");
        Relator lDirectRel = doAdd(this, aInFromCat, aInFromGName,
                                   RelatorCat.getCreateToDirect(
                                           this.getName(), this.getCardinality(), true),
                                   aInToCat, aInToGName);

        //System.out.println("### Relator.add() : added direct: " + lDirectRel);

        RelatorCat lInvFromRelatorCat = RelatorCat.getCreateFromInverse(
                this.getName(),
                Cardinality.MULTI, // YES, INVERSE IS ALWAYS MULTI this.getCardinality(),
                true);

        RelatorCat lInvToRelatorCat = RelatorCat.getCreateToInverse(
                this.getName(),
                Cardinality.MULTI, // YES, INVERSE IS ALWAYS MULTI this.getCardinality(),
                true);


        Relator lInvRelator = doAdd(
                lInvFromRelatorCat, aInToCat, aInToGName,
                lInvToRelatorCat, aInFromCat, aInFromGName);

        //System.out.println("### Relator.add() : added inverse: " + lInvRelator);

        return lDirectRel;
    }

    private static synchronized Relator doAdd(
            RelatorCat aInFromRelatorCat,
            Cat aInFromCat,
            String aInFromGName,
            RelatorCat aInToRelatorCat,
            Cat aInToCat,
            String aInToGName)
    {
        //System.out.println("#### Relator.doAdd(" + aInFromRelatorCat + "," + aInFromCat + ", " + aInFromGName + ", "  + ", " + aInToRelatorCat + ", " + aInToCat + ", " + aInToGName + ")");

        Relator lFromRelator = aInFromRelatorCat.getRelator(aInFromGName);//aInFromRelatorCat.getNodes().getItem(aInFromGName);

        if (null == lFromRelator)
        {
            lFromRelator = new Relator(
                    aInFromRelatorCat,
                    aInToRelatorCat,
                    null,
                    aInFromGName,
                    aInFromCat,
                    aInToCat,
                    aInFromGName); // YES, FROM.

            //System.out.println("#### Relator.doAdd() added from: " + lFromRelator);
        }
        else if (aInFromRelatorCat.getCardinality() != lFromRelator.getCardinality())
        {
            Severity.DEATH.report(lFromRelator.toString(), "register reln", "duplicate source", "for dest: " + aInToGName + "; CARDINALITY MISMATCH");
        }
        else if (RelatorCat.Type.FROM != lFromRelator.getType())
        {
            Severity.DEATH.report(lFromRelator.toString(), "register reln", "duplicate source", "for dest: " + aInToGName + "; TYPE MISMATCH");
        }

        Relator lToRelator = (Relator) lFromRelator.getNode().getChildItem(aInToRelatorCat, aInToGName);
        if (null == lToRelator)
        {
            lToRelator = new Relator(
                    aInToRelatorCat,
                    aInFromRelatorCat,
                    lFromRelator,
                    aInToGName,
                    aInToCat,
                    aInFromCat,
                    aInToGName);

            //System.out.println("#### Relator.doAdd() added to: " + lToRelator);
        }
        else if (aInToRelatorCat.getCardinality() != lToRelator.getCardinality())
        {
            Severity.DEATH.report(lToRelator.toString(), "register reln", "duplicate dest", "for source: " + aInFromGName + "; CARDINALITY MISMATCH");
        }
        else if (RelatorCat.Type.TO != lFromRelator.getType())
        {
            Severity.DEATH.report(lToRelator.toString(), "register reln", "duplicate dest", "for source: " + aInFromGName + "; TYPE MISMATCH");
        }
        //System.out.println("#### Relator.doAdd() : END\n\n");
        return lToRelator;
    }


    /**
     * Category registration.
     * @param aIn category to be registered
     */
    private static synchronized void register(RelatorCat aIn)
    {
        if (null != nameToCatTable.put(aIn.getName(), aIn))
        {
            Severity.DEATH.report(aIn.toString(), "register", "duplicate", "name already defined; must be unique");
        }
        if (null != idToCatTable.put(aIn.getId(), aIn))
        {
            Severity.DEATH.report(aIn.toString(), "register", "duplicate", "id already defined; must be unique");
        }
    }

    private RelatorCat(
            String aInName,
            Cardinality aInCardinality,
            Type aInType,
            Direction aInDirection)
    {
        super(aInName);
        cardinality = aInCardinality;
        type = aInType;
        direction = aInDirection;
        RelatorCat.register(this);
    }

    private final Cardinality cardinality;
    private final Type type;
    private final Direction direction;
    private static final Map<String, RelatorCat> nameToCatTable = new TreeMap<>();
    private static final Map<Number, RelatorCat> idToCatTable = new TreeMap<>();
}
