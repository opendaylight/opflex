package org.opendaylight.opflex.genie.content.model.mrelator;

import org.opendaylight.opflex.genie.content.model.mclass.MClass;
import org.opendaylight.opflex.genie.content.model.mcont.MContained;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameComponent;
import org.opendaylight.opflex.genie.content.model.mnaming.MNameRule;
import org.opendaylight.opflex.genie.content.model.mnaming.MNamer;
import org.opendaylight.opflex.genie.content.model.module.Module;
import org.opendaylight.opflex.genie.engine.model.Cat;
import org.opendaylight.opflex.genie.engine.model.Item;
import org.opendaylight.opflex.genie.engine.model.Pair;
import org.opendaylight.opflex.modlan.utils.Strings;

/**
 * Created by midvorki on 8/6/14.
 */
public class MRelationship extends Item
{
    public static final Cat MY_CAT = Cat.getCreate("rel:relationship");

    public MRelationship(
            MTarget aInParent,
            String aInName,
            RelatorType aInType,
            PointCardinality aInSourceCard,
            PointCardinality aInTargetCard)
    {
        super(MY_CAT, aInParent, aInName);
        String lSrcClassGName = getSourceClassGName();
        int lSlashIdx = lSrcClassGName.indexOf('/');
        moduleName = lSrcClassGName.substring(0, lSlashIdx);
        sourceClassLocalName = lSrcClassGName.substring(lSlashIdx + 1, lSrcClassGName.length());
        type = aInType;
        sourceCardinality = aInSourceCard;
        targetCardinality = aInTargetCard;
        sourceRelnClass = initSourceRelnClass();
        //targetRelnClass = initTargetRelnClass();
        resolverRelnClass = initResolverRelnClass();
    }


    public String getSourceClassGName() { return getMRelator().getTargetGName(); }
    public MClass getSourceClass() { return getMRelator().getTarget(); }

    public String getTargetClassGName() { return getMTarget().getTargetGName(); }
    public MClass getTargetClass(){ return getMTarget().getTarget(); }

    public boolean hasSourceRelnClass() { return type.hasSourceObject(); }
    public MClass getSourceRelnClass() { return sourceRelnClass; }
    public boolean hasTargetRelnClass() { return type.hasTargetObject(); }
    //public MClass getTargetRelnClass() { return targetRelnClass; }
    public boolean hasResolverRelnClass() { return type.hasResolverObject(); }
    public MClass getResolverRelnClass() { return resolverRelnClass; }

    public MSource getMSource() { return getMTarget().getMSource(); }
    public MTarget getMTarget() { return (MTarget) getParent(); }
    public MRelator getMRelator() { return getMTarget().getRelator(); }
    public MRelated getMRelated() { return getMTarget().getMRelated(); }

    public RelatorType getType() { return type; }
    public PointCardinality getSourceCardinality() { return sourceCardinality; }
    public PointCardinality getTargetCardinality() { return targetCardinality; }

    /*
        source: module/ReSrc<LocalClassName><Name>
        target: module/ReTgt<LocalClassName><Name>
        resolver: module/ReRes<LocalClassName><Name>
     */

    private MClass initSourceRelnClass()
    {
        MClass lClass = null;
        if (type.hasSourceObject())
        {
            // CLASS NAME FORMAT: module/ReSrc<LocalClassName><Name>
            Pair<MRelationshipClass,Boolean> lRes = initClass("RSrc", "To", type.isNamed() ? "relator/NameResolvedRelSource" : "relator/DirectRelSource");
            lClass = lRes.getFirst();

            MContained.addRule(getSourceClassGName(), lClass.getGID().getName());

            if (lRes.getSecond()) // IS NEW
            {
                MNamer lNamer = MNamer.get(lClass.getGID().getName(), true);
                MNameRule lNr = lNamer.getNameRule(Strings.ASTERISK,true);

                switch (getSourceCardinality())
                {
                    case SINGLE:

                        new MNameComponent(lNr,getLID().getName(), null);
                        break;

                    case MANY:

                        // TODO: FIX NAMING: IF ONLY ONE CLASS IS CONTAINED IN THIS RELN, NO NEED FOR CLASS, JUST NEED NAME...
                        new MNameComponent(lNr, getLID().getName(), "targetClass");
                        new MNameComponent(lNr, null, "targetName");
                        break;
                }
            }
            // TODO: PROPERTIES
        }
        return lClass;
    }

    private MClass initTargetRelnClass()
    {
        MClass lClass = null;
        if (type.hasTargetObject())
        {
            // CLASS NAME FORMAT: module/ReTgt<LocalClassName><Name>
            Pair<MRelationshipClass,Boolean> lRes =  initClass("RTgt", "From", "relator/Target");
            lClass = lRes.getFirst();
            MContained.addRule(getTargetClassGName(), lClass.getGID().getName());

            if (lRes.getSecond()) // IS NEW
            {
                MNamer lNamer = MNamer.get(lClass.getGID().getName(), true);
                MNameRule lNr = lNamer.getNameRule(Strings.ASTERISK,true);

                switch (getTargetCardinality())
                {
                    case SINGLE:

                        new MNameComponent(lNr,getLID().getName(), null);
                        break;

                    case MANY:

                        new MNameComponent(lNr, getLID().getName(), "source");
                        break;
                }
            }
        }
        return lClass;
    }

    private MClass initResolverRelnClass()
    {
        MClass lClass = null;
        if (type.hasTargetObject())
        {
            // CLASS NAME FORMAT: module/ReRes<LocalClassName><Name>
            Pair<MRelationshipClass,Boolean> lRes = initClass("RRes", "To", "relator/Resolver");
            lClass = lRes.getFirst();

            // TODO: WHAT SHOULD IT BE PARENTED BY: FOR NOW, DEFAULTED IN MODEL TO RELATOR/UNIVERSE

            // TODO: PROPERTIES

            // TODO: ADD NAMING
            MNamer lNamer = MNamer.get(lClass.getGID().getName(), true);
            MNameRule lNr = lNamer.getNameRule(Strings.ASTERISK,true);
        }
        return lClass;
    }

    private Pair<MRelationshipClass,Boolean> initClass(String aInClassPrefix, String aInClassSuffix, String aInSuperClass)
    {
        String lClassName = sourceClassLocalName + aInClassSuffix + Strings.upFirstLetter(getLID().getName()) + aInClassPrefix;
        Module lModule = Module.get(moduleName, true);
        MRelationshipClass lClass = (MRelationshipClass) lModule.getChildItem(MRelationshipClass.MY_CAT,lClassName);
        boolean isNew = null == lClass;
        if (isNew)
        {
            lClass = new MRelationshipClass(lModule, lClassName, this);
            lClass.addSuperclass(aInSuperClass);
        }
        else
        {
            lClass.addTargetRelationship(this);
            //Severity.WARN.report(toString(), "init relationship class", "already exists: " + lClass, "");
        }
        return new Pair<MRelationshipClass,Boolean>(lClass,isNew);
    }

    private final RelatorType type;
    private final PointCardinality sourceCardinality;
    private final PointCardinality targetCardinality;
    private final MClass sourceRelnClass;
    //private final MClass targetRelnClass;
    private final MClass resolverRelnClass;
    private final String moduleName;
    private final String sourceClassLocalName;
}
