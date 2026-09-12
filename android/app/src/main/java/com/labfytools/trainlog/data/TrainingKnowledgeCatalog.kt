package com.labfytools.trainlog.data

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale

enum class KnowledgeConfidence { HIGH, MODERATE, UNCERTAIN }
enum class MuscleEntityType { MUSCLE, MUSCLE_REGION, MUSCLE_GROUP }
enum class ExerciseKnowledgeStatus { RESOLVED_FAMILY_VARIANT_LIMITED, CONDITIONAL, UNRESOLVED }
enum class BodyZoneAuditStatus { CONFIRMED, QUESTIONABLE, UNRESOLVED }
enum class BodyZoneAuditSeverity { NONE, INFORMATION }
enum class EquipmentScienceStatus {
    SCIENTIFICALLY_DOCUMENTED,
    MECHANICALLY_IDENTIFIED_ANATOMY_INCOMPLETE,
    EQUIPMENT_IDENTITY_UNCERTAIN,
}
enum class MuscleRole { PRIMARY, SECONDARY, STABILIZERS }

data class ScienceReference(
    val refId: String, val title: String, val authorsOrOrganization: String, val year: Int?,
    val type: String, val url: String, val topics: List<String>, val notes: String,
    val limitations: String, val doi: String?, val pmid: String?, val accessedOn: String,
    val publicationNote: String?,
)

data class MuscleKnowledge(
    val muscleId: String, val displayName: String, val displayNameFr: String,
    val entityType: MuscleEntityType, val anatomicalGroup: String, val aggregateGroupId: String?,
    val bodyZoneId: String, val bodyZoneIds: List<String>, val jointActionIds: List<String>,
    val primaryActions: List<String>, val primaryActionsSemantics: String,
    val overlapWarning: String, val functionalNotes: String, val confidence: KnowledgeConfidence,
    val evidenceType: String, val sourceRefs: List<String>, val memberMuscleIds: List<String>,
)

data class JointActionKnowledge(
    val actionId: String, val displayNameFr: String, val definition: String,
    val anatomicalRegion: String, val jointComplex: String, val jointOrComplex: String,
    val principalPlane: String, val planeNotes: String, val contributingMuscleIds: List<String>,
    val contributorSemantics: String, val evidenceType: String, val confidence: KnowledgeConfidence,
    val sourceRefs: List<String>, val notes: String,
)

data class MovementPatternKnowledge(
    val patternId: String, val displayNameFr: String, val definition: String,
    val typicalActionIds: List<String>, val typicalBodyZoneIds: List<String>,
    val evidenceType: String, val confidence: KnowledgeConfidence, val sourceRefs: List<String>,
    val notes: String, val bodyZoneSemantics: String?, val parentPatternId: String?,
)

data class ExerciseInterpretation(
    val familyDescription: String, val actionIds: List<String>, val patternIds: List<String>,
    val primaryMuscleIds: List<String>, val secondaryMuscleIds: List<String>,
    val stabilizerMuscleIds: List<String>, val primaryZoneId: String,
    val secondaryZoneIds: List<String>, val confidence: KnowledgeConfidence,
    val evidenceType: String, val sourceRefs: List<String>, val variantNotes: String,
    val roleNotes: String, val requiredConfirmation: String? = null,
) {
    fun muscles(role: MuscleRole): List<String> = when (role) {
        MuscleRole.PRIMARY -> primaryMuscleIds
        MuscleRole.SECONDARY -> secondaryMuscleIds
        MuscleRole.STABILIZERS -> stabilizerMuscleIds
    }
}

data class ScientificBodyZoneMapping(val primaryZoneId: String, val secondaryZoneIds: List<String>)
data class ExistingBodyZoneMapping(val primaryZoneId: String?, val secondaryZoneIds: List<String>)

data class BodyZoneAudit(
    val status: BodyZoneAuditStatus, val severity: BodyZoneAuditSeverity,
    val confidence: KnowledgeConfidence, val rationale: String,
    val existingPrimaryZoneId: String?, val existingSecondaryZoneIds: List<String>,
    val proposedMutation: String?, val sourceRefs: List<String>,
)

data class ExerciseKnowledge(
    val exerciseId: String, val exerciseName: String, val equipmentIds: List<String>,
    val identityEvidence: String, val identityEvidenceType: String,
    val resolutionStatus: ExerciseKnowledgeStatus, val confidence: KnowledgeConfidence,
    val interpretation: ExerciseInterpretation?, val conditionalInterpretation: ExerciseInterpretation?,
    val existingBodyZones: ExistingBodyZoneMapping, val sourceRefs: List<String>,
    val limitations: List<String>, val equipmentLinkStatus: String?, val bodyZoneAudit: BodyZoneAudit,
)

data class EquipmentCapabilityKnowledge(
    val displayName: String, val exerciseIds: List<String>, val linkStatus: String,
    val interpretation: ExerciseInterpretation?, val requirements: String,
)

data class EquipmentKnowledge(
    val equipmentId: String, val manufacturer: String?, val model: String?,
    val identificationStatus: String, val scienceStatus: EquipmentScienceStatus,
    val catalogType: String, val catalogLoadSemantics: EquipmentLoadSemantics?,
    val mechanics: String, val evidenceType: String, val confidence: KnowledgeConfidence,
    val sourceRefs: List<String>, val capabilities: List<EquipmentCapabilityKnowledge>,
    val requiresActualExercise: Boolean, val limitations: List<String>, val auditNote: String?,
    val scientificStatusScope: String,
)

data class EquipmentExerciseRelation(
    val equipmentId: String, val exerciseId: String, val relationType: String,
    val configurationLabel: String?, val confidence: KnowledgeConfidence,
    val sourceRefs: List<String>,
)

data class KnowledgeExerciseFilters(
    val movementPatternId: String? = null,
    val muscleId: String? = null,
    val muscleRole: MuscleRole? = null,
    val scientificZoneId: String? = null,
    val includeZoneDescendants: Boolean = true,
    val equipmentId: String? = null,
)

/**
 * Immutable training-knowledge view over the shared repository assets.
 * WHY: strict loading makes malformed scientific data a deployment failure and
 * prevents Android from quietly developing a second, hand-maintained taxonomy.
 */
class TrainingKnowledgeCatalog private constructor(
    val references: List<ScienceReference>, val muscles: List<MuscleKnowledge>,
    val jointActions: List<JointActionKnowledge>, val movementPatterns: List<MovementPatternKnowledge>,
    val exercises: List<ExerciseKnowledge>, val equipment: List<EquipmentKnowledge>,
    val equipmentExerciseRelations: List<EquipmentExerciseRelation>,
    private val bodyZones: BodyZoneCatalog,
) {
    private val referencesById = references.associateBy { it.refId }
    private val musclesById = muscles.associateBy { it.muscleId }
    private val actionsById = jointActions.associateBy { it.actionId }
    private val patternsById = movementPatterns.associateBy { it.patternId }
    private val exercisesById = exercises.associateBy { it.exerciseId }
    private val equipmentById = equipment.associateBy { it.equipmentId }
    private val relationsByEquipment = equipmentExerciseRelations.groupBy { it.equipmentId }
    private val relationsByExercise = equipmentExerciseRelations.groupBy { it.exerciseId }

    fun getReference(refId: String) = referencesById[refId]
    fun getMuscle(muscleId: String) = musclesById[muscleId]
    fun getJointAction(actionId: String) = actionsById[actionId]
    fun getMovementPattern(patternId: String) = patternsById[patternId]
    fun getExerciseKnowledge(exerciseId: String): ExerciseKnowledge? = exercisesById[exerciseId]
    fun getConditionalExerciseKnowledge(exerciseId: String): ExerciseKnowledge? =
        exercisesById[exerciseId]?.takeIf { it.resolutionStatus == ExerciseKnowledgeStatus.CONDITIONAL }
    fun getEquipmentKnowledge(equipmentId: String): EquipmentKnowledge? = equipmentById[equipmentId]

    fun listExercisesForEquipment(equipmentId: String): List<ExerciseKnowledge> {
        require(equipmentById.containsKey(equipmentId)) { "equipment_id inconnu: $equipmentId" }
        return relationsByEquipment[equipmentId].orEmpty().map { exercisesById.getValue(it.exerciseId) }
    }

    fun listRelationsForEquipment(equipmentId: String): List<EquipmentExerciseRelation> =
        relationsByEquipment[equipmentId].orEmpty()

    fun listEquipmentForExercise(exerciseId: String): List<EquipmentKnowledge> {
        require(exercisesById.containsKey(exerciseId)) { "exercise_id inconnu: $exerciseId" }
        return relationsByExercise[exerciseId].orEmpty().map { equipmentById.getValue(it.equipmentId) }
    }

    fun listEquipmentForBodyZone(zoneId: String): List<EquipmentKnowledge> {
        val exerciseIds = queryExercises(KnowledgeExerciseFilters(scientificZoneId = zoneId))
            .map { it.exerciseId }.toSet()
        // CONTRACT: BODY ZONE membership comes from resolved exercise anatomy,
        // never from an equipment name, capability label, or custom record.
        return equipment.filter { item -> relationsByEquipment[item.equipmentId].orEmpty()
            .any { it.exerciseId in exerciseIds } }
    }

    fun getScientificBodyZoneMapping(exerciseId: String): ScientificBodyZoneMapping? =
        resolvedInterpretation(exerciseId)?.let { ScientificBodyZoneMapping(it.primaryZoneId, it.secondaryZoneIds) }

    fun listExercisesByMovementPattern(patternId: String): List<ExerciseKnowledge> {
        require(patternsById.containsKey(patternId)) { "pattern_id inconnu: $patternId" }
        return queryExercises(KnowledgeExerciseFilters(movementPatternId = patternId))
    }

    fun listExercisesByMuscle(muscleId: String, role: MuscleRole): List<ExerciseKnowledge> {
        require(musclesById.containsKey(muscleId)) { "muscle_id inconnu: $muscleId" }
        return queryExercises(KnowledgeExerciseFilters(muscleId = muscleId, muscleRole = role))
    }

    fun listCompatibleExercises(equipmentId: String): List<ExerciseKnowledge> {
        require(equipmentById.containsKey(equipmentId)) { "equipment_id inconnu: $equipmentId" }
        return listExercisesForEquipment(equipmentId)
    }

    fun queryExercises(filters: KnowledgeExerciseFilters): List<ExerciseKnowledge> {
        filters.movementPatternId?.let { require(patternsById.containsKey(it)) { "pattern_id inconnu: $it" } }
        filters.muscleId?.let { require(musclesById.containsKey(it)) { "muscle_id inconnu: $it" } }
        require((filters.muscleId == null) == (filters.muscleRole == null)) {
            "muscleId et muscleRole doivent être fournis ensemble"
        }
        filters.equipmentId?.let { require(equipmentById.containsKey(it)) { "equipment_id inconnu: $it" } }
        val zones = filters.scientificZoneId?.let {
            require(bodyZones.lookup(it) != null) { "zone_id inconnu: $it" }
            if (filters.includeZoneDescendants) bodyZones.descendantsAndSelf(it) else setOf(it)
        }
        return exercises.filter { exercise ->
            val interpretation = resolvedInterpretation(exercise.exerciseId) ?: return@filter false
            (filters.movementPatternId == null || filters.movementPatternId in interpretation.patternIds) &&
                (filters.muscleId == null || filters.muscleId in interpretation.muscles(filters.muscleRole!!)) &&
                (filters.equipmentId == null || relationsByEquipment[filters.equipmentId].orEmpty()
                    .any { it.exerciseId == exercise.exerciseId }) &&
                (zones == null || interpretation.primaryZoneId in zones ||
                    interpretation.secondaryZoneIds.any { it in zones })
        }
    }

    private fun resolvedInterpretation(exerciseId: String): ExerciseInterpretation? =
        exercisesById[exerciseId]?.takeIf {
            it.resolutionStatus == ExerciseKnowledgeStatus.RESOLVED_FAMILY_VARIANT_LIMITED
        }?.interpretation

    companion object {
        private val confidenceValues = KnowledgeConfidence.entries.associateBy { it.name.lowercase(Locale.ROOT) }
        private val idPattern = Regex("[a-z][a-z0-9_]*")
        private val exerciseIdPattern = Regex("ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")
        private val equipmentIdPattern = Regex("(?:[a-z][a-z0-9_]*|eq_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12})")
        private val scientificReferenceTypes = setOf("established_anatomy", "emg_evidence", "intervention_evidence")

        fun load(context: Context): TrainingKnowledgeCatalog = load(
            context = context,
            bodyZones = BodyZoneCatalog.load(context),
        )

        internal fun load(context: Context, bodyZones: BodyZoneCatalog): TrainingKnowledgeCatalog {
            return load({ name -> context.assets.open(name).bufferedReader().use { it.readText() } }, bodyZones)
        }

        internal fun load(assetText: (String) -> String, bodyZones: BodyZoneCatalog): TrainingKnowledgeCatalog {
            fun root(name: String, format: String, collection: String): JSONArray {
                val text = assetText(name)
                DuplicateJsonKeyValidator.validate(text)
                val value = JSONObject(text)
                requireKeys(value, setOf("format", "version", collection), collection)
                check(value.getString("format") == format) { "$collection: format non pris en charge" }
                check(value.rawInt("version") == 1) { "$collection: version non prise en charge" }
                return value.getJSONArray(collection)
            }
            val referenceJson = root("science-references-v1.json", "trainlog-science-references-v1", "references")
            val muscleJson = root("muscles-v1.json", "trainlog-muscles-v1", "muscles")
            val actionJson = root("joint-actions-v1.json", "trainlog-joint-actions-v1", "joint_actions")
            val patternJson = root("movement-patterns-v1.json", "trainlog-movement-patterns-v1", "movement_patterns")
            val exerciseJson = root("exercise-knowledge-v1.json", "trainlog-exercise-knowledge-v1", "exercises")
            val equipmentJson = root("equipment-knowledge-v1.json", "trainlog-equipment-knowledge-v1", "equipment")
            val relationText = assetText("equipment-exercise-relations-v2.json")
            DuplicateJsonKeyValidator.validate(relationText)
            val relationRoot = JSONObject(relationText)
            requireKeys(relationRoot, setOf("format", "version", "equipment_relations"), "equipment_relations")
            check(relationRoot.getString("format") == "trainlog-equipment-exercise-relations-v2" &&
                relationRoot.rawInt("version") == 2) { "equipment_relations: version non prise en charge" }
            val relationJson = relationRoot.getJSONArray("equipment_relations")

            val references = referenceJson.objects("ref_id") { item ->
                requireKeys(item, setOf("ref_id", "title", "authors_or_organization", "year", "type", "url", "topics", "notes", "limitations", "doi", "pmid", "accessed_on"), "reference", setOf("publication_note"))
                ScienceReference(item.text("ref_id"), item.text("title"), item.text("authors_or_organization"), item.nullableInt("year"), item.text("type"), item.text("url"), item.strings("topics"), item.text("notes"), item.text("limitations"), item.nullableString("doi"), item.nullableString("pmid"), item.text("accessed_on"), item.optionalString("publication_note"))
            }
            val refIds = references.map { it.refId }.toSet()
            val referenceTypes = references.associate { it.refId to it.type }

            data class RawMuscle(val item: JSONObject)
            val rawMuscles = muscleJson.objects("muscle_id") { RawMuscle(it) }
            val muscleIds = rawMuscles.map { it.item.text("muscle_id") }.toSet()
            val actionIds = actionJson.ids("action_id")
            val patternIds = patternJson.ids("pattern_id")
            val zoneIds = bodyZones.zones.map { it.zoneId }.toSet()
            val equipmentIds = equipmentJson.ids("equipment_id")
            val exerciseIds = exerciseJson.ids("exercise_id")
            // CONTRACT: V1 catalogs are additive; identity, ordering, and cross-reference
            // validation below define integrity without freezing today's collection sizes.

            val muscles = rawMuscles.map { raw ->
                val item = raw.item
                val required = setOf("muscle_id", "display_name", "display_name_fr", "entity_type", "anatomical_group", "aggregate_group_id", "body_zone_id", "body_zone_ids", "joint_action_ids", "primary_actions", "primary_actions_semantics", "overlap_warning", "functional_notes", "confidence", "evidence_type", "source_refs")
                requireKeys(item, required, "muscle", setOf("member_muscle_ids"))
                val aggregate = item.nullableString("aggregate_group_id")
                check(aggregate == null || aggregate in muscleIds) { "aggregate_group_id pendant" }
                val members = item.optionalStrings("member_muscle_ids")
                check(members.all { it in muscleIds }) { "membre musculaire pendant" }
                val zones = item.strings("body_zone_ids"); val actions = item.strings("joint_action_ids"); val primaryActions = item.strings("primary_actions"); val refs = item.strings("source_refs", true)
                check(item.text("body_zone_id") in zoneIds && zones.all { it in zoneIds } && actions.all { it in actionIds } && primaryActions.all { it in actionIds } && refs.all { it in refIds }) { "référence croisée muscle invalide" }
                MuscleKnowledge(item.text("muscle_id"), item.text("display_name"), item.text("display_name_fr"), enumValue(item.text("entity_type"), MuscleEntityType.entries), item.text("anatomical_group"), aggregate, item.text("body_zone_id"), zones, actions, primaryActions, item.text("primary_actions_semantics"), item.text("overlap_warning"), item.string("functional_notes"), confidence(item), item.text("evidence_type"), refs, members)
            }

            val actions = actionJson.objects("action_id") { item ->
                requireKeys(item, setOf("action_id", "display_name_fr", "definition", "anatomical_region", "joint_complex", "joint_or_complex", "principal_plane", "plane_notes", "contributing_muscle_ids", "contributor_semantics", "evidence_type", "confidence", "source_refs", "notes"), "joint_action")
                val contributors = item.strings("contributing_muscle_ids"); val refs = item.strings("source_refs", true)
                check(contributors.all { it in muscleIds } && refs.all { it in refIds }) { "référence croisée action invalide" }
                JointActionKnowledge(item.text("action_id"), item.text("display_name_fr"), item.text("definition"), item.text("anatomical_region"), item.text("joint_complex"), item.text("joint_or_complex"), item.text("principal_plane"), item.text("plane_notes"), contributors, item.text("contributor_semantics"), item.text("evidence_type"), confidence(item), refs, item.text("notes"))
            }
            val patterns = patternJson.objects("pattern_id") { item ->
                val required = setOf("pattern_id", "display_name_fr", "definition", "typical_action_ids", "typical_body_zone_ids", "evidence_type", "confidence", "source_refs", "notes")
                requireKeys(item, required, "movement_pattern", setOf("body_zone_semantics", "parent_pattern_id"))
                val typicalActions = item.strings("typical_action_ids"); val typicalZones = item.strings("typical_body_zone_ids"); val refs = item.strings("source_refs", true)
                check(typicalActions.all { it in actionIds } && typicalZones.all { it in zoneIds } && refs.all { it in refIds }) { "référence croisée pattern invalide" }
                val parent = item.optionalString("parent_pattern_id"); check(parent == null || parent in patternIds) { "parent_pattern_id pendant" }
                MovementPatternKnowledge(item.text("pattern_id"), item.text("display_name_fr"), item.text("definition"), typicalActions, typicalZones, item.text("evidence_type"), confidence(item), refs, item.text("notes"), item.optionalString("body_zone_semantics"), parent)
            }
            fun interpretation(item: JSONObject, conditional: Boolean): ExerciseInterpretation {
                val required = setOf("family_description", "action_ids", "pattern_ids", "primary_muscle_ids", "secondary_muscle_ids", "stabilizer_muscle_ids", "primary_zone_id", "secondary_zone_ids", "confidence", "evidence_type", "source_refs", "variant_notes", "role_notes")
                requireKeys(item, required, "interpretation", if (conditional) setOf("required_confirmation") else emptySet())
                val actionRefs = item.strings("action_ids"); val patternRefs = item.strings("pattern_ids")
                val primary = item.strings("primary_muscle_ids"); val secondary = item.strings("secondary_muscle_ids"); val stabilizers = item.strings("stabilizer_muscle_ids")
                check((primary + secondary + stabilizers).size == (primary + secondary + stabilizers).toSet().size) { "muscle présent dans plusieurs rôles" }
                val secondaryZones = item.strings("secondary_zone_ids"); val refs = item.strings("source_refs", true); val primaryZone = item.text("primary_zone_id")
                check(actionRefs.all { it in actionIds } && patternRefs.all { it in patternIds } && (primary + secondary + stabilizers).all { it in muscleIds } && primaryZone in zoneIds && secondaryZones.all { it in zoneIds } && primaryZone !in secondaryZones && refs.all { it in refIds }) { "référence croisée interprétation invalide" }
                return ExerciseInterpretation(item.text("family_description"), actionRefs, patternRefs, primary, secondary, stabilizers, primaryZone, secondaryZones, confidence(item), item.text("evidence_type"), refs, item.text("variant_notes"), item.text("role_notes"), item.optionalString("required_confirmation"))
            }
            val exercises = exerciseJson.objects("exercise_id") { item ->
                val required = setOf("exercise_id", "exercise_name", "equipment_ids", "identity_evidence", "identity_evidence_type", "resolution_status", "confidence", "interpretation", "conditional_interpretation", "existing_body_zones", "source_refs", "limitations", "body_zone_audit")
                requireKeys(item, required, "exercise", setOf("equipment_link_status"))
                val status = enumValue(item.text("resolution_status"), ExerciseKnowledgeStatus.entries)
                val regular = item.nullableObject("interpretation")?.let { interpretation(it, false) }
                val conditional = item.nullableObject("conditional_interpretation")?.let { interpretation(it, true) }
                check((status == ExerciseKnowledgeStatus.RESOLVED_FAMILY_VARIANT_LIMITED && regular != null && conditional == null) || (status == ExerciseKnowledgeStatus.CONDITIONAL && regular == null && conditional != null) || (status == ExerciseKnowledgeStatus.UNRESOLVED && regular == null && conditional == null)) { "interprétation incompatible avec resolution_status" }
                val equipmentRefs = item.strings("equipment_ids"); val refs = item.strings("source_refs")
                check(status == ExerciseKnowledgeStatus.UNRESOLVED || refs.isNotEmpty()) { "exercice résolu/conditionnel sans source" }
                check(equipmentRefs.all { it in equipmentIds } && refs.all { it in refIds }) { "référence croisée exercice invalide" }
                val existing = item.getJSONObject("existing_body_zones"); requireKeys(existing, setOf("primary_zone_id", "secondary_zone_ids"), "existing_body_zones")
                val existingZones = ExistingBodyZoneMapping(existing.nullableString("primary_zone_id"), existing.strings("secondary_zone_ids")); check((existingZones.primaryZoneId == null || existingZones.primaryZoneId in zoneIds) && existingZones.secondaryZoneIds.all { it in zoneIds }) { "zone existante pendante" }
                val audit = item.getJSONObject("body_zone_audit")
                requireKeys(audit, setOf("status", "severity", "confidence", "rationale", "existing_primary_zone_id", "existing_secondary_zone_ids", "proposed_mutation", "source_refs"), "body_zone_audit")
                val auditPrimary = audit.nullableString("existing_primary_zone_id")
                val auditSecondary = audit.strings("existing_secondary_zone_ids")
                val auditRefs = audit.strings("source_refs")
                check((auditPrimary == null || auditPrimary in zoneIds) && auditSecondary.all { it in zoneIds }) { "zone d'audit pendante" }
                check(auditRefs.all { it in refIds }) { "référence d'audit pendante" }
                // INVARIANT: every non-unresolved BODY ZONE conclusion remains traceable
                // to evidence, matching the canonical cross-catalog validator.
                check(audit.text("status") == "unresolved" || auditRefs.isNotEmpty()) { "audit BODY ZONE résolu sans source" }
                val bodyZoneAudit = BodyZoneAudit(
                    enumValue(audit.text("status"), BodyZoneAuditStatus.entries),
                    enumValue(audit.text("severity"), BodyZoneAuditSeverity.entries),
                    confidence(audit), audit.text("rationale"), auditPrimary, auditSecondary,
                    audit.nullableString("proposed_mutation"), auditRefs,
                )
                ExerciseKnowledge(item.text("exercise_id"), item.text("exercise_name"), equipmentRefs, item.text("identity_evidence"), item.text("identity_evidence_type"), status, confidence(item), regular, conditional, existingZones, refs, item.strings("limitations"), item.optionalString("equipment_link_status"), bodyZoneAudit)
            }
            val linked = mutableSetOf<Pair<String, String>>()
            val equipment = equipmentJson.objects("equipment_id") { item ->
                val required = setOf("equipment_id", "manufacturer", "model", "identification_status", "scientific_status", "catalog_type", "catalog_load_semantics", "mechanics", "evidence_type", "confidence", "source_refs", "capabilities", "requires_actual_exercise", "limitations", "scientific_status_scope")
                requireKeys(item, required, "equipment", setOf("audit_note"))
                val refs = item.strings("source_refs", true); check(refs.all { it in refIds }) { "référence équipement pendante" }
                val conf = confidence(item); check(!(conf == KnowledgeConfidence.HIGH && item.text("evidence_type") == "manufacturer_statement")) { "cartographie anatomique élevée fondée seulement sur fabricant" }
                // CONTRACT: HIGH equipment mappings require at least one admissible
                // scientific source type; Android must accept exactly the validator policy.
                check(conf != KnowledgeConfidence.HIGH || refs.any { referenceTypes[it] in scientificReferenceTypes }) { "cartographie équipement élevée sans source scientifique" }
                val capabilities = item.getJSONArray("capabilities").objectsUnordered { capability ->
                    requireKeys(capability, setOf("display_name", "exercise_ids", "link_status", "interpretation", "requirements"), "capability")
                    val capabilityExercises = capability.strings("exercise_ids"); check(capabilityExercises.all { it in exerciseIds }) { "exercice fantôme dans capability" }
                    check(capability.text("link_status") in setOf("linked_existing", "linked_identity_conditional_interpretation", "unlinked_capability", "catalog_compatible_not_observed_occurrence")) { "link_status invalide" }
                    capabilityExercises.forEach { linked += it to item.text("equipment_id") }
                    // WHY: a one-sided capability would make desktop and Android
                    // compatibility queries disagree over the same immutable assets.
                    capabilityExercises.forEach { exerciseId ->
                        check(item.text("equipment_id") in exercises.single { it.exerciseId == exerciseId }.equipmentIds) { "compatibilité équipement/exercice asymétrique" }
                    }
                    EquipmentCapabilityKnowledge(capability.text("display_name"), capabilityExercises, capability.text("link_status"), capability.nullableObject("interpretation")?.let { interpretation(it, false) }, capability.text("requirements"))
                }
                EquipmentKnowledge(item.text("equipment_id"), item.nullableString("manufacturer"), item.nullableString("model"), item.text("identification_status"), enumValue(item.text("scientific_status"), EquipmentScienceStatus.entries), item.text("catalog_type"), item.nullableString("catalog_load_semantics")?.let { enumValue(it, EquipmentLoadSemantics.entries) }, item.text("mechanics"), item.text("evidence_type"), conf, refs, capabilities, item.rawBoolean("requires_actual_exercise"), item.strings("limitations"), item.optionalString("audit_note"), item.text("scientific_status_scope"))
            }
            exercises.forEach { exercise -> exercise.equipmentIds.forEach { check(exercise.exerciseId to it in linked) { "compatibilité exercice/équipement asymétrique" } } }
            val relationPairs = mutableSetOf<Pair<String, String>>()
            var previousEquipmentId: String? = null
            val relations = relationJson.objectsUnordered { group ->
                requireKeys(group, setOf("equipment_id", "exercise_options"), "equipment relation")
                val equipmentId = group.text("equipment_id")
                check(equipmentId in equipmentIds && (previousEquipmentId == null || previousEquipmentId!! < equipmentId)) {
                    "equipment relation inconnue, dupliquée ou hors ordre"
                }
                previousEquipmentId = equipmentId
                var previousOption: String? = null
                group.getJSONArray("exercise_options").objectsUnordered { option ->
                    requireKeys(option, setOf("exercise_id", "relation_type", "configuration_label", "confidence", "source_refs"), "equipment exercise option")
                    val exerciseId = option.text("exercise_id")
                    val configuration = option.nullableString("configuration_label")
                    val key = "$exerciseId|${configuration ?: ""}"
                    val refs = option.strings("source_refs", true)
                    check(exerciseId in exerciseIds && option.text("relation_type") == "supported_exercise" &&
                        refs.all { it in refIds } && relationPairs.add(equipmentId to exerciseId) &&
                        (previousOption == null || previousOption!! < key)) { "relation équipement/exercice invalide" }
                    previousOption = key
                    EquipmentExerciseRelation(equipmentId, exerciseId, option.text("relation_type"),
                        configuration, confidence(option), refs)
                }
            }.flatten()
            check(relations.none { it.exerciseId == "ex_617007f9-7420-4408-91b9-8ffb77900f13" }) {
                "Seated Leg reste non résolu et distinct de la flexion de genou assise"
            }
            return TrainingKnowledgeCatalog(references, muscles, actions, patterns, exercises, equipment, relations, bodyZones)
        }

        private fun confidence(item: JSONObject): KnowledgeConfidence = confidenceValues[item.text("confidence")] ?: error("confidence invalide")
        private fun <T : Enum<T>> enumValue(value: String, entries: List<T>): T = entries.firstOrNull { it.name.lowercase(Locale.ROOT) == value } ?: error("valeur enum invalide: $value")
        private fun requireKeys(value: JSONObject, required: Set<String>, where: String, optional: Set<String> = emptySet()) {
            val keys = value.keys().asSequence().toSet(); check(required.all { it in keys } && keys.all { it in required || it in optional }) { "$where: clés invalides" }
        }
        private fun JSONObject.text(key: String): String = get(key).let { check(it is String && it.isNotBlank()) { "$key: texte requis" }; it }
        private fun JSONObject.string(key: String): String = get(key).let { check(it is String) { "$key: chaîne attendue" }; it }
        private fun JSONObject.nullableString(key: String): String? = if (isNull(key)) null else get(key).let { check(it is String) { "$key: chaîne attendue" }; it }
        private fun JSONObject.optionalString(key: String): String? = if (!has(key) || isNull(key)) null else nullableString(key)
        private fun JSONObject.rawInt(key: String): Int = get(key).let { check(it is Int) { "$key: entier attendu" }; it }
        private fun JSONObject.nullableInt(key: String): Int? = if (isNull(key)) null else rawInt(key)
        private fun JSONObject.rawBoolean(key: String): Boolean = get(key).let { check(it is Boolean) { "$key: booléen attendu" }; it }
        private fun JSONObject.nullableObject(key: String): JSONObject? = if (isNull(key)) null else get(key).let { check(it is JSONObject) { "$key: objet attendu" }; it }
        private fun JSONObject.strings(key: String, nonempty: Boolean = false): List<String> = get(key).let { value ->
            check(value is JSONArray) { "$key: tableau attendu" }; List(value.length()) { index -> value.get(index).let { check(it is String && it.isNotBlank()) { "$key: chaîne vide/invalide" }; it } }.also {
                check(it.size == it.toSet().size && (!nonempty || it.isNotEmpty())) { "$key: doublon ou tableau vide" }
                if (key.endsWith("_ids") || key == "source_refs" || key == "joint_action_ids" || key == "primary_actions") {
                    check(it == it.sorted()) { "$key: ordre stable requis" }
                }
            }
        }
        private fun JSONObject.optionalStrings(key: String): List<String> = if (has(key)) strings(key) else emptyList()
        private fun JSONArray.ids(key: String): Set<String> = objects(key) { it.text(key) }.toSet()
        private fun <T> JSONArray.objects(key: String, transform: (JSONObject) -> T): List<T> {
            val previous = mutableSetOf<String>(); var last: String? = null
            return objectsUnordered { item ->
                val id = item.text(key)
                val validId = when (key) {
                    "exercise_id" -> exerciseIdPattern.matches(id)
                    "equipment_id" -> equipmentIdPattern.matches(id)
                    else -> idPattern.matches(id)
                }
                check(validId) { "$key invalide" }
                check(previous.add(id) && (last == null || last!! < id)) { "$key dupliqué ou hors ordre" }
                last = id
                transform(item)
            }
        }
        private fun <T> JSONArray.objectsUnordered(transform: (JSONObject) -> T): List<T> = List(length()) { index -> get(index).let { check(it is JSONObject) { "objet attendu" }; transform(it) } }
    }
}

/** Small lexical pass because org.json otherwise silently accepts duplicate object keys. */
private object DuplicateJsonKeyValidator {
    fun validate(text: String) { Parser(text).parse() }
    private class Parser(private val source: String) {
        private var index = 0
        fun parse() { value(); whitespace(); check(index == source.length) { "JSON suffixe invalide" } }
        private fun value() { whitespace(); check(index < source.length) { "JSON tronqué" }; when (source[index]) { '{' -> objectValue(); '[' -> arrayValue(); '"' -> string(); 't' -> literal("true"); 'f' -> literal("false"); 'n' -> literal("null"); else -> number() } }
        private fun objectValue() { index++; whitespace(); val keys = mutableSetOf<String>(); if (take('}')) return; while (true) { whitespace(); val key = string(); check(keys.add(key)) { "clé JSON dupliquée: $key" }; whitespace(); expect(':'); value(); whitespace(); if (take('}')) return; expect(',') } }
        private fun arrayValue() { index++; whitespace(); if (take(']')) return; while (true) { value(); whitespace(); if (take(']')) return; expect(',') } }
        private fun string(): String { expect('"'); val out = StringBuilder(); while (index < source.length) { val c = source[index++]; when (c) { '"' -> return out.toString(); '\\' -> { check(index < source.length); val escaped = source[index++]; if (escaped == 'u') { check(index + 4 <= source.length); val hex = source.substring(index, index + 4); check(hex.all { it.isDigit() || it.lowercaseChar() in 'a'..'f' }); out.append(hex.toInt(16).toChar()); index += 4 } else { check(escaped in "\"\\/bfnrt"); out.append(escaped) } }; else -> { check(c.code >= 0x20); out.append(c) } } }; error("chaîne JSON tronquée") }
        private fun literal(value: String) { check(source.startsWith(value, index)); index += value.length }
        private fun number() { val start = index; if (take('-')) Unit; check(index < source.length && source[index].isDigit()); if (source[index] == '0') index++ else while (index < source.length && source[index].isDigit()) index++; if (take('.')) { check(index < source.length && source[index].isDigit()); while (index < source.length && source[index].isDigit()) index++ }; if (index < source.length && source[index] in "eE") { index++; if (index < source.length && source[index] in "+-") index++; check(index < source.length && source[index].isDigit()); while (index < source.length && source[index].isDigit()) index++ }; check(index > start) }
        private fun whitespace() { while (index < source.length && source[index].isWhitespace()) index++ }
        private fun take(c: Char): Boolean { if (index < source.length && source[index] == c) { index++; return true }; return false }
        private fun expect(c: Char) { check(take(c)) { "JSON: '$c' attendu à $index" } }
    }
}
