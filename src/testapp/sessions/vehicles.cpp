/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "vehicles.h"

#include <adera/activescene/vehicles_vb_fn.h>
#include <adera/drawing/CameraController.h>
#include <adera/machines/links.h>

#include <osp/activescene/basic.h>
#include <osp/activescene/physics.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing.h>
#include <osp/util/UserInputHandler.h>


using namespace adera;

using namespace osp::active;
using namespace osp::draw;
using namespace osp::link;
using namespace osp;

using osp::restypes::gc_importer;

using namespace Magnum::Math::Literals;

namespace testapp::scenes
{

Session setup_parts(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              scene)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);

    auto const tgScn = scene.get_pipelines<PlScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_PARTS);
    auto const tgParts = out.create_pipelines<PlParts>(rBuilder);

    out.m_cleanup = tgScn.cleanup;

    rBuilder.pipeline(tgParts.partIds)          .parent(tgScn.update);
    rBuilder.pipeline(tgParts.partPrefabs)      .parent(tgScn.update);
    rBuilder.pipeline(tgParts.partTransformWeld).parent(tgScn.update);
    rBuilder.pipeline(tgParts.partDirty)        .parent(tgScn.update);
    rBuilder.pipeline(tgParts.weldIds)          .parent(tgScn.update);
    rBuilder.pipeline(tgParts.weldDirty)        .parent(tgScn.update);
    rBuilder.pipeline(tgParts.machIds)          .parent(tgScn.update);
    rBuilder.pipeline(tgParts.nodeIds)          .parent(tgScn.update);
    rBuilder.pipeline(tgParts.connect)          .parent(tgScn.update);
    rBuilder.pipeline(tgParts.mapWeldPart)      .parent(tgScn.update);
    rBuilder.pipeline(tgParts.mapPartMach)      .parent(tgScn.update);
    rBuilder.pipeline(tgParts.mapPartActive)    .parent(tgScn.update);
    rBuilder.pipeline(tgParts.mapWeldActive)    .parent(tgScn.update);

    auto &rScnParts = top_emplace< ACtxParts >      (topData, idScnParts);
    auto &rUpdMach  = top_emplace< UpdMachPerType > (topData, idUpdMach);

    // Resize containers to fit all existing MachTypeIds and NodeTypeIds
    // These Global IDs are dynamically initialized just as the program starts
    bitvector_resize(rUpdMach.m_machTypesDirty, MachTypeReg_t::size());
    rUpdMach.m_localDirty           .resize(MachTypeReg_t::size());
    rScnParts.m_machines.m_perType  .resize(MachTypeReg_t::size());
    rScnParts.m_nodePerType         .resize(NodeTypeReg_t::size());

    auto const idNull = lgrn::id_null<TopDataId>();

    rBuilder.task()
        .name       ("Clear Vehicle Spawning vector after use")
        .run_on     ({tgScn.cleanup(Run_)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts,           idResources})
        .func([] (ACtxParts& rScnParts, Resources& rResources) noexcept
    {
        for (osp::PrefabPair &rPrefabPair : rScnParts.m_partPrefabs)
        {
            rResources.owner_destroy(gc_importer, std::move(rPrefabPair.m_importer));
        }
    });


    rBuilder.task()
        .name       ("Clear Part dirty vectors after use")
        .run_on     ({tgParts.partDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts})
        .func([] (ACtxParts& rScnParts) noexcept
    {
        rScnParts.m_partDirty.clear();
    });

    rBuilder.task()
        .name       ("Clear Weld dirty vectors after use")
        .run_on     ({tgParts.weldDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts})
        .func([] (ACtxParts& rScnParts) noexcept
    {
        rScnParts.m_weldDirty.clear();
    });

    return out;
} // setup_parts

Session setup_vehicle_spawn(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene)
{
    auto const tgScn = scene.get_pipelines<PlScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_VEHICLE_SPAWN);
    auto const tgVhSp = out.create_pipelines<PlVehicleSpawn>(rBuilder);

    rBuilder.pipeline(tgVhSp.spawnRequest)  .parent(tgScn.update);
    rBuilder.pipeline(tgVhSp.spawnedParts)  .parent(tgScn.update);
    rBuilder.pipeline(tgVhSp.spawnedWelds)  .parent(tgScn.update);
    rBuilder.pipeline(tgVhSp.rootEnts)      .parent(tgScn.update);
    rBuilder.pipeline(tgVhSp.spawnedMachs)     .parent(tgScn.update);

    top_emplace< ACtxVehicleSpawn >     (topData, idVehicleSpawn);

    rBuilder.task()
        .name       ("Schedule Vehicle spawn")
        .schedules  ({tgVhSp.spawnRequest(Schedule_)})
        .sync_with  ({tgScn.update(Run)})
        .push_to    (out.m_tasks)
        .args       ({                   idVehicleSpawn })
        .func([] (ACtxVehicleSpawn const& rVehicleSpawn) noexcept -> TaskActions
    {
        return rVehicleSpawn.spawnRequest.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rBuilder.task()
        .name       ("Clear Vehicle Spawning vector after use")
        .run_on     ({tgVhSp.spawnRequest(Clear)})
        .push_to    (out.m_tasks)
        .args       ({             idVehicleSpawn})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn) noexcept
    {
        rVehicleSpawn.spawnRequest.clear();
    });

    return out;
} // setup_vehicle_spawn

Session setup_vehicle_spawn_vb(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              prefabs,
        Session const&              parts,
        Session const&              vehicleSpawn,
        Session const&              signalsFloat)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(prefabs,       TESTAPP_DATA_PREFABS);
    //OSP_DECLARE_GET_DATA_IDS(signalsFloat,  TESTAPP_DATA_SIGNALS_FLOAT);
    OSP_DECLARE_GET_DATA_IDS(vehicleSpawn,  TESTAPP_DATA_VEHICLE_SPAWN);
    auto const tgCS     = commonScene   .get_pipelines<PlCommonScene>();
    auto const tgPf     = prefabs       .get_pipelines<PlPrefabs>();
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgParts  = parts         .get_pipelines<PlParts>();
    auto const tgVhSp   = vehicleSpawn  .get_pipelines<PlVehicleSpawn>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_VEHICLE_SPAWN_VB);
    auto const tgVhSpVB = out.create_pipelines<PlVehicleSpawnVB>(rBuilder);

    rBuilder.pipeline(tgVhSpVB.dataVB)      .parent(tgScn.update);
    rBuilder.pipeline(tgVhSpVB.remapParts)  .parent(tgScn.update);
    rBuilder.pipeline(tgVhSpVB.remapWelds)  .parent(tgScn.update);
    rBuilder.pipeline(tgVhSpVB.remapMachs)  .parent(tgScn.update);
    rBuilder.pipeline(tgVhSpVB.remapNodes)  .parent(tgScn.update);

    top_emplace< ACtxVehicleSpawnVB >(topData, idVehicleSpawnVB);

    rBuilder.task()
        .name       ("Create PartIds and WeldIds for vehicles to spawn from VehicleData")
        .run_on     ({tgVhSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgVhSp.spawnedParts(Resize), tgVhSpVB.remapParts(Modify_), tgVhSpVB.remapWelds(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({             idVehicleSpawn,                    idVehicleSpawnVB,           idScnParts})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        SysVehicleSpawnVB::create_parts_and_welds(rVehicleSpawn, rVehicleSpawnVB, rScnParts);
    });

    rBuilder.task()
        .name       ("Request prefabs for vehicle parts from VehicleBuilder")
        .run_on     ({tgVhSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnRequest(Modify_), tgVhSp.spawnedParts(UseOrRun)})
        .push_to    (out.m_tasks)
        .args       ({             idVehicleSpawn,                          idVehicleSpawnVB,           idScnParts,             idPrefabs,           idResources})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabs& rPrefabs, Resources& rResources) noexcept
    {
        SysVehicleSpawnVB::request_prefabs(rVehicleSpawn, rVehicleSpawnVB, rScnParts, rPrefabs, rResources);
    });


    rBuilder.task()
        .name       ("Create Machine IDs copied from VehicleData")
        .run_on     ({tgVhSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgVhSpVB.dataVB(UseOrRun), tgVhSpVB.remapMachs(Modify_), tgVhSp.spawnedMachs(Resize), tgParts.machIds(New)})
        .push_to    (out.m_tasks)
        .args       ({             idVehicleSpawn,                    idVehicleSpawnVB,           idScnParts})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB &rVSVB = rVehicleSpawnVB;

        // Count total machines, and calculate offsets for remaps.

        std::size_t machTotal = 0;
        std::size_t remapMachTotal = 0;

        rVSVB.machtypeCount.clear();
        rVSVB.machtypeCount.resize(MachTypeReg_t::size(), 0);

        rVSVB.remapMachOffsets.resize(newVehicleCount);

        for (SpVehicleId vhId{0}; vhId.value < newVehicleCount; ++vhId.value)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            Machines const &srcMachines = pVData->m_machines;
            std::size_t const bounds = srcMachines.m_ids.capacity();

            rVSVB.remapMachOffsets[vhId] = remapMachTotal;

            remapMachTotal += bounds;
            machTotal += srcMachines.m_ids.size();

            for (MachTypeId type = 0; type < MachTypeReg_t::size(); ++type)
            {
                rVSVB.machtypeCount[type] += srcMachines.m_perType[type].m_localIds.size();
            }
        }

        rVehicleSpawn.spawnedMachs.resize(machTotal);
        rVSVB.remapMachs.resize(remapMachTotal);

        // Create ACtxParts MachAny/LocalIDs and populate remaps

        // MachAnyIDs created here
        rScnParts.m_machines.m_ids.create(rVehicleSpawn.spawnedMachs.begin(),
                                          rVehicleSpawn.spawnedMachs.end());

        rScnParts.m_machines.m_machToLocal.resize(rScnParts.m_machines.m_ids.capacity());

        auto itDstMachIds = rVehicleSpawn.spawnedMachs.cbegin();

        for (SpVehicleId vhId{0}; vhId.value < newVehicleCount; ++vhId.value)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            Machines const &srcMachines = pVData->m_machines;

            std::size_t const remapMachOffset = rVSVB.remapMachOffsets[vhId];

            for (MachAnyId const srcMach : srcMachines.m_ids.bitview().zeros())
            {
                MachAnyId const dstMach = *itDstMachIds;
                ++itDstMachIds;

                // Populate map for "VehicleBuilder MachAnyId -> ACtxParts MachAnyId"
                rVSVB.remapMachs[remapMachOffset + srcMach] = dstMach;

                // Create ACtxParts MachLocalIds
                // MachLocalIds don't need a remap, since they can be obtained
                // from a MachAnyId.
                // TODO: This can be optimized later, where all local IDs are
                //       created at once with ids.create(first, last), and make
                //       resize(..) called once per type too
                MachTypeId const    type            = srcMachines.m_machTypes[srcMach];
                PerMachType&        rDstPerType     = rScnParts.m_machines.m_perType[type];

                MachLocalId const dstLocal = rDstPerType.m_localIds.create();
                rDstPerType.m_localToAny.resize(rDstPerType.m_localIds.capacity());

                rDstPerType.m_localToAny[dstLocal] = dstMach;
                rScnParts.m_machines.m_machToLocal[dstMach] = dstLocal;
            }
        }
    });

    rBuilder.task()
        .name       ("Update Part<->Machine maps")
        .run_on     ({tgVhSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgVhSpVB.dataVB(UseOrRun), tgVhSpVB.remapMachs(UseOrRun), tgVhSpVB.remapParts(UseOrRun), tgParts.mapPartMach(New)})
        .push_to    (out.m_tasks)
        .args       ({             idVehicleSpawn,                          idVehicleSpawnVB,           idScnParts,             idPrefabs,           idResources})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabs& rPrefabs, Resources& rResources) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB const& rVSVB = rVehicleSpawnVB;

        for (SpVehicleId vhId{0}; vhId.value < newVehicleCount; ++vhId.value)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            rScnParts.m_machineToPart.resize(rScnParts.m_machines.m_ids.capacity());
            rScnParts.m_partToMachines.ids_reserve(rScnParts.m_partIds.capacity());
            rScnParts.m_partToMachines.data_reserve(rScnParts.m_machines.m_ids.capacity());

            std::size_t const remapMachOffset = rVSVB.remapMachOffsets[vhId];
            std::size_t const remapPartOffset = rVSVB.remapPartOffsets[vhId];

            // Update rScnParts machine->part map
            for (MachAnyId const srcMach : pVData->m_machines.m_ids.bitview().zeros())
            {
                MachAnyId const dstMach = rVSVB.remapMachs[remapMachOffset + srcMach];
                PartId const    srcPart = pVData->m_machToPart[srcMach];
                PartId const    dstPart = rVSVB.remapParts[remapPartOffset + srcPart];

                rScnParts.m_machineToPart[dstMach] = dstPart;
            }

            // Update rScnParts part->machine multimap
            for (PartId const srcPart : pVData->m_partIds.bitview().zeros())
            {
                PartId const dstPart = rVSVB.remapParts[remapPartOffset + srcPart];

                auto const& srcPairs = pVData->m_partToMachines[srcPart];

                rScnParts.m_partToMachines.emplace(dstPart, srcPairs.size());
                auto dstPairs = rScnParts.m_partToMachines[dstPart];

                for (int i = 0; i < srcPairs.size(); ++i)
                {
                    MachinePair const&  srcPair  = srcPairs[i];
                    MachinePair&        rDstPair = dstPairs[i];
                    MachAnyId const     srcMach  = pVData->m_machines.m_perType[srcPair.m_type].m_localToAny[srcPair.m_local];
                    MachAnyId const     dstMach  = rVSVB.remapMachs[remapMachOffset + srcMach];
                    MachTypeId const    dstType  = srcPair.m_type;
                    MachLocalId const   dstLocal = rScnParts.m_machines.m_machToLocal[dstMach];

                    rDstPair = { .m_local = dstLocal, .m_type = dstType };
                }
            }
        }
    });

    rBuilder.task()
        .name       ("Create (and connect) Node IDs copied from VehicleBuilder")
        .run_on     ({tgVhSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgVhSpVB.dataVB(UseOrRun), tgVhSpVB.remapMachs(UseOrRun), tgVhSpVB.remapNodes(Modify_), tgParts.nodeIds(New), tgParts.connect(New)})
        .push_to    (out.m_tasks)
        .args       ({             idVehicleSpawn,                    idVehicleSpawnVB,           idScnParts})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB &rVSVB = rVehicleSpawnVB;

        rVSVB.remapNodeOffsets.resize(newVehicleCount * NodeTypeReg_t::size());
        auto remapNodeOffsets2d = rVSVB.remap_node_offsets_2d();

        // Add up bounds needed for all nodes of every type for remaps
        std::size_t remapNodeTotal = 0;
        for (VehicleData const* pVData : rVSVB.dataVB)
        {
            if (pVData == nullptr)
            {
                continue;
            }
            for (PerNodeType const &rSrcNodeType : pVData->m_nodePerType)
            {
                remapNodeTotal += rSrcNodeType.m_nodeIds.capacity();
            }
        }
        rVSVB.remapNodes.resize(remapNodeTotal);

        std::size_t nodeRemapUsed = 0;

        for (SpVehicleId vhId{0}; vhId.value < newVehicleCount; ++vhId.value)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            auto machRemap = arrayView(std::as_const(rVSVB.remapMachs)).exceptPrefix(rVSVB.remapMachOffsets[vhId]);

            for (NodeTypeId nodeType = 0; nodeType < NodeTypeReg_t::size(); ++nodeType)
            {
                PerNodeType const &rSrcNodeType = pVData->m_nodePerType[nodeType];

                std::size_t const remapSize = rSrcNodeType.m_nodeIds.capacity();
                auto nodeRemapOut = arrayView(rVSVB.remapNodes).sliceSize(nodeRemapUsed, remapSize);
                remapNodeOffsets2d[vhId.value][nodeType] = nodeRemapUsed;
                nodeRemapUsed += remapSize;
                copy_nodes(rSrcNodeType, pVData->m_machines, machRemap,
                           rScnParts.m_nodePerType[nodeType], rScnParts.m_machines, nodeRemapOut);
            }
        }
    });

    rBuilder.task()
        .name       ("Update PartId<->ActiveEnt mapping")
        .run_on     ({tgVhSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgVhSp.spawnedParts(UseOrRun), tgPf.spawnedEnts(UseOrRun), tgParts.mapPartActive(Modify)})
        .push_to    (out.m_tasks)
        .args       ({             idVehicleSpawn,                 idBasic,           idScnParts,              idPrefabs})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxBasic const& rBasic, ACtxParts& rScnParts,  ACtxPrefabs& rPrefabs) noexcept
    {
        rScnParts.m_partToActive.resize(rScnParts.m_partIds.capacity());
        rScnParts.m_activeToPart.resize(rBasic.m_activeIds.capacity());

        // Populate PartId<->ActiveEnt mapping, now that the prefabs exist

        auto itPrefab = rVehicleSpawn.spawnedPrefabs.begin();

        for (PartId const partId : rVehicleSpawn.spawnedParts)
        {
            ActiveEnt const root = rPrefabs.spawnedEntsOffset[*itPrefab].front();
            ++itPrefab;

            rScnParts.m_partToActive[partId]            = root;
            rScnParts.m_activeToPart[std::size_t(root)] = partId;
        }
    });

/*
    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVbNodeReq, tgSigFloatLinkReq}).data(
            "Copy float signal values from VehicleBuilder",
            TopDataIds_t{                  idVehicleSpawn,                          idVehicleSpawnVB,           idScnParts,                       idSigValFloat},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, SignalValues_t<float>& rSigValFloat) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB const &rVSVB = rVehicleSpawnVB;

        if (newVehicleCount == 0)
        {
            return;
        }

        auto const remapNodeOffsets2d = rVSVB.remap_node_offsets_2d();

        for (NewVehicleId vhId = 0; vhId < newVehicleCount; ++vhId)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            PerNodeType const&  srcFloatNodes       = pVData->m_nodePerType[gc_ntSigFloat];
            entt::any const&    srcFloatValuesAny   = srcFloatNodes.m_nodeValues;
            auto const&         srcFloatValues      = entt::any_cast< SignalValues_t<float> >(srcFloatValuesAny);
            std::size_t const   nodeRemapOffset     = remapNodeOffsets2d[vhId][gc_ntSigFloat];
            auto const          nodeRemap           = arrayView(rVSVB.remapNodes).exceptPrefix(nodeRemapOffset);

            for (NodeId const srcNode : srcFloatNodes.m_nodeIds.bitview().zeros())
            {
                NodeId const dstNode = nodeRemap[srcNode];
                rSigValFloat[dstNode] = srcFloatValues[srcNode];
            }
        }
    }));
    */

    return out;
} // setup_vehicle_spawn_vb

Session setup_vehicle_spawn_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              sceneRenderer,
        Session const&              vehicleSpawn)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(vehicleSpawn,  TESTAPP_DATA_VEHICLE_SPAWN);
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const tgVhSp   = vehicleSpawn  .get_pipelines< PlVehicleSpawn >();

    Session out;

    rBuilder.task()
        .name       ("asdf")
        .run_on     ({tgVhSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgVhSp.rootEnts(UseOrRun), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,                        idVehicleSpawn})
        .func([] (ACtxSceneRender& rScnRender, ACtxVehicleSpawn const& rVehicleSpawn) noexcept
    {
        for (ActiveEnt const ent : rVehicleSpawn.rootEnts)
        {
            rScnRender.m_needDrawTf.set(std::size_t(ent));
        }
    });

    return out;
} // setup_vehicle_spawn_draw

#if 0

Session setup_signals_float(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              commonScene,
        Session const&              parts)
{
    OSP_SESSION_UNPACK_TAGS(commonScene,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(parts,      TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,      TESTAPP_PARTS);

    Session signalsFloat;
    OSP_SESSION_ACQUIRE_DATA(signalsFloat, topData, TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_ACQUIRE_TAGS(signalsFloat, rTags,   TESTAPP_SIGNALS_FLOAT);

    rBuilder.tag(tgSigFloatLinkReq)         .depend_on({tgSigFloatLinkMod});
    rBuilder.tag(tgSigFloatUpdReq)          .depend_on({tgSigFloatLinkMod, tgSigFloatUpdMod});

    top_emplace< SignalValues_t<float> >    (topData, idSigValFloat);
    top_emplace< UpdateNodes<float> >       (topData, idSigUpdFloat);

    // NOTE: Eventually have an array of UpdateNodes to allow multiple threads to update nodes in
    //       parallel, noting the use of "Reduce". Tag limits are intended select which UpdateNodes
    //       are passed to each thread, once they're properly implemented.

    auto const idNull = lgrn::id_null<TopDataId>();


    signalsFloat.task() = rBuilder.task().assign({tgSceneEvt, tgNodeUpdEvt, tgSigFloatUpdEvt, tgSigFloatUpdReq, tgMachUpdEnqMod}).data(
            "Reduce Signal-Float Nodes",
            TopDataIds_t{                   idNull,                   idSigUpdFloat,                       idSigValFloat,                idUpdMach,                    idMachUpdEnqueue,                 idScnParts,                       idMachEvtTags },
            wrap_args([] (WorkerContext const ctx, UpdateNodes<float>& rSigUpdFloat, SignalValues_t<float>& rSigValFloat, UpdMachPerType& rUpdMach, std::vector<TagId>& rMachUpdEnqueue, ACtxParts const& rScnParts, MachTypeToEvt_t const& rMachEvtTags ) noexcept
    {
        if ( ! rSigUpdFloat.m_dirty )
        {
            return; // Not dirty, nothing to do
        }

        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];

        // NOTE: The various use of reset() clear entire bit arrays, which may or may
        //       not be expensive. They likely use memset

        for (std::size_t const machTypeDirty : rUpdMach.m_machTypesDirty.ones())
        {
            rUpdMach.m_localDirty[machTypeDirty].reset();
        }
        rUpdMach.m_machTypesDirty.reset();

        // Sees which nodes changed, and writes into rUpdMach set dirty which MACHINES
        // must be updated next
        update_signal_nodes<float>(
                rSigUpdFloat.m_nodeDirty.ones(),
                rFloatNodes.m_nodeToMach,
                rScnParts.m_machines,
                arrayView(rSigUpdFloat.m_nodeNewValues),
                rSigValFloat,
                rUpdMach);
        rSigUpdFloat.m_nodeDirty.reset();
        rSigUpdFloat.m_dirty = false;

        // Tasks cannot be enqueued here directly, since that will interfere with other node reduce
        // tasks. All machine tasks must be enqueued at the same time. rMachUpdEnqueue here is
        // passed to a task in setup_parts

        // Run tasks needed to update machine types that are dirty
        for (MachTypeId const type : rUpdMach.m_machTypesDirty.ones())
        {
            rMachUpdEnqueue.push_back(rMachEvtTags[type]);
        }
    }));

    signalsFloat.task() = rBuilder.task().assign({tgSceneEvt, tgLinkReq, tgSigFloatLinkMod}).data(
            "Allocate Signal-Float Node Values",
            TopDataIds_t{                    idSigUpdFloat,                       idSigValFloat,                 idScnParts  },
            wrap_args([] (UpdateNodes<float>& rSigUpdFloat, SignalValues_t<float>& rSigValFloat, ACtxParts const& rScnParts) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];
        rSigUpdFloat.m_nodeNewValues.resize(rFloatNodes.m_nodeIds.capacity());
        rSigUpdFloat.m_nodeDirty.ints().resize(rFloatNodes.m_nodeIds.vec().capacity());
        rSigValFloat.resize(rFloatNodes.m_nodeIds.capacity());
    }));

    return signalsFloat;
}

template <MachTypeId const& MachType_T>
TopTaskFunc_t gen_allocate_mach_bitsets()
{
    static TopTaskFunc_t const func = wrap_args([] (ACtxParts& rScnParts, UpdMachPerType& rUpdMach) noexcept
    {
        rUpdMach.m_localDirty[MachType_T].ints().resize(rScnParts.m_machines.m_perType[MachType_T].m_localIds.vec().capacity());
    });

    return func;
}

Session setup_mach_rocket(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              commonScene,
        Session const&              parts,
        Session const&              signalsFloat)
{
    OSP_SESSION_UNPACK_TAGS(commonScene,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT)
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);

    using namespace adera;

    Session machRocket;
    OSP_SESSION_ACQUIRE_TAGS(machRocket, rTags,   TESTAPP_MACH_ROCKET);

    top_get< MachTypeToEvt_t >(topData, idMachEvtTags).at(gc_mtMagicRocket) = tgMhRocketEvt;

    machRocket.task() = rBuilder.task().assign({tgSceneEvt, tgLinkReq, tgLinkMhUpdMod}).data(
            "Allocate Machine update bitset for MagicRocket",
            TopDataIds_t{idScnParts, idUpdMach},
            gen_allocate_mach_bitsets<gc_mtMagicRocket>());

    return machRocket;
}

Session setup_mach_rcsdriver(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              commonScene,
        Session const&              parts,
        Session const&              signalsFloat)
{
    OSP_SESSION_UNPACK_TAGS(commonScene,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT)
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);

    using namespace adera;

    Session machRcsDriver;
    OSP_SESSION_ACQUIRE_TAGS(machRcsDriver, rTags,   TESTAPP_MACH_RCSDRIVER);

    top_get< MachTypeToEvt_t >(topData, idMachEvtTags).at(gc_mtRcsDriver) = tgMhRcsDriverEvt;

    machRcsDriver.task() = rBuilder.task().assign({tgMhRcsDriverEvt, tgSigFloatUpdMod}).data(
            "RCS Drivers calculate new values",
            TopDataIds_t{           idScnParts,                      idUpdMach,                       idSigValFloat,                    idSigUpdFloat },
            wrap_args([] (ACtxParts& rScnParts, UpdMachPerType const& rUpdMach, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];
        PerMachType &rRockets = rScnParts.m_machines.m_perType[gc_mtRcsDriver];

        for (MachLocalId const local : rUpdMach.m_localDirty[gc_mtRcsDriver].ones())
        {
            MachAnyId const mach = rRockets.m_localToAny[local];
            lgrn::Span<NodeId const> const portSpan = rFloatNodes.m_machToNode[mach];

            NodeId const thrNode = connected_node(portSpan, ports_rcsdriver::gc_throttleOut.m_port);
            if (thrNode == lgrn::id_null<NodeId>())
            {
                continue; // Throttle Output not connected, calculations below are useless
            }

            auto const rcs_read = [&rSigValFloat, portSpan] (float& rDstVar, PortEntry const& entry)
            {
                NodeId const node = connected_node(portSpan, entry.m_port);

                if (node != lgrn::id_null<NodeId>())
                {
                    rDstVar = rSigValFloat[node];
                }
            };

            Vector3 pos{0.0f};
            Vector3 dir{0.0f};
            Vector3 cmdLin{0.0f};
            Vector3 cmdAng{0.0f};

            rcs_read( pos.x(),    ports_rcsdriver::gc_posXIn    );
            rcs_read( pos.y(),    ports_rcsdriver::gc_posYIn    );
            rcs_read( pos.z(),    ports_rcsdriver::gc_posZIn    );
            rcs_read( dir.x(),    ports_rcsdriver::gc_dirXIn    );
            rcs_read( dir.y(),    ports_rcsdriver::gc_dirYIn    );
            rcs_read( dir.z(),    ports_rcsdriver::gc_dirZIn    );
            rcs_read( cmdLin.x(), ports_rcsdriver::gc_cmdLinXIn );
            rcs_read( cmdLin.y(), ports_rcsdriver::gc_cmdLinYIn );
            rcs_read( cmdLin.z(), ports_rcsdriver::gc_cmdLinZIn );
            rcs_read( cmdAng.x(), ports_rcsdriver::gc_cmdAngXIn );
            rcs_read( cmdAng.y(), ports_rcsdriver::gc_cmdAngYIn );
            rcs_read( cmdAng.z(), ports_rcsdriver::gc_cmdAngZIn );

            OSP_LOG_TRACE("RCS controller {} pitch = {}", local, cmdAng.x());
            OSP_LOG_TRACE("RCS controller {} yaw = {}", local, cmdAng.y());
            OSP_LOG_TRACE("RCS controller {} roll = {}", local, cmdAng.z());

            float const thrCurr = rSigValFloat[thrNode];
            float const thrNew = thruster_influence(pos, dir, cmdLin, cmdAng);

            if (thrCurr != thrNew)
            {
                rSigUpdFloat.assign(thrNode, thrNew);
            }
        }
    }));

    machRcsDriver.task() = rBuilder.task().assign({tgSceneEvt, tgLinkReq, tgLinkMhUpdMod}).data(
            "Allocate Machine update bitset for RCS Drivers",
            TopDataIds_t{idScnParts, idUpdMach},
            gen_allocate_mach_bitsets<gc_mtRcsDriver>());

    return machRcsDriver;
}
#endif


struct VehicleTestControls
{
    MachLocalId m_selectedUsrCtrl{lgrn::id_null<MachLocalId>()};

    input::EButtonControlIndex m_btnSwitch;
    input::EButtonControlIndex m_btnThrMax;
    input::EButtonControlIndex m_btnThrMin;
    input::EButtonControlIndex m_btnThrMore;
    input::EButtonControlIndex m_btnThrLess;
    input::EButtonControlIndex m_btnPitchUp;
    input::EButtonControlIndex m_btnPitchDn;
    input::EButtonControlIndex m_btnYawLf;
    input::EButtonControlIndex m_btnYawRt;
    input::EButtonControlIndex m_btnRollLf;
    input::EButtonControlIndex m_btnRollRt;
};


Session setup_vehicle_control(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              scene,
        Session const&              parts,
        Session const&              signalsFloat)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    //OSP_DECLARE_GET_DATA_IDS(signalsFloat,  TESTAPP_DATA_SIGNALS_FLOAT)
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(windowApp,     TESTAPP_DATA_WINDOW_APP);
    //OSP_DECLARE_GET_DATA_IDS(app,           TESTAPP_DATA_APP);
    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScn    = scene         .get_pipelines<PlScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_VEHICLE_CONTROL);
    auto const tgVhCtrl = out.create_pipelines<PlVehicleCtrl>(rBuilder);

    rBuilder.pipeline(tgVhCtrl.selectedVehicle).parent(tgScn.update);

    auto &rUserInput = top_get< input::UserInputHandler >(topData, idUserInput);

    // TODO: add cleanup task
    top_emplace<VehicleTestControls>(topData, idVhControls, VehicleTestControls{
        .m_btnSwitch    = rUserInput.button_subscribe("game_switch"),
        .m_btnThrMax    = rUserInput.button_subscribe("vehicle_thr_max"),
        .m_btnThrMin    = rUserInput.button_subscribe("vehicle_thr_min"),
        .m_btnThrMore   = rUserInput.button_subscribe("vehicle_thr_more"),
        .m_btnThrLess   = rUserInput.button_subscribe("vehicle_thr_less"),
        .m_btnPitchUp   = rUserInput.button_subscribe("vehicle_pitch_up"),
        .m_btnPitchDn   = rUserInput.button_subscribe("vehicle_pitch_dn"),
        .m_btnYawLf     = rUserInput.button_subscribe("vehicle_yaw_lf"),
        .m_btnYawRt     = rUserInput.button_subscribe("vehicle_yaw_rt"),
        .m_btnRollLf    = rUserInput.button_subscribe("vehicle_roll_lf"),
        .m_btnRollRt    = rUserInput.button_subscribe("vehicle_roll_rt")
    });

    auto const idNull = lgrn::id_null<TopDataId>();

    rBuilder.task()
        .name       ("Select vehicle")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgVhCtrl.selectedVehicle(Modify)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts,                               idUserInput,                     idVhControls})
        .func([] (ACtxParts& rScnParts, input::UserInputHandler const &rUserInput, VehicleTestControls &rVhControls) noexcept
    {
        PerMachType &rUsrCtrl    = rScnParts.m_machines.m_perType[gc_mtUserCtrl];

        // Select a UsrCtrl machine when pressing the switch button
        if (rUserInput.button_state(rVhControls.m_btnSwitch).m_triggered)
        {
            ++rVhControls.m_selectedUsrCtrl;
            bool found = false;
            for (MachLocalId local = rVhControls.m_selectedUsrCtrl; local < rUsrCtrl.m_localIds.capacity(); ++local)
            {
                if (rUsrCtrl.m_localIds.exists(local))
                {
                    found = true;
                    rVhControls.m_selectedUsrCtrl = local;
                    break;
                }
            }

            if ( ! found )
            {
                rVhControls.m_selectedUsrCtrl = lgrn::id_null<MachLocalId>();
                OSP_LOG_INFO("Unselected vehicles");
            }
            else
            {
                OSP_LOG_INFO("Selected User Control: {}", rVhControls.m_selectedUsrCtrl);
            }
        }

        if (rVhControls.m_selectedUsrCtrl == lgrn::id_null<MachLocalId>())
        {
            return; // No vehicle selected
        }
    });

/*
    rBuilder.task()
        .name       ("Write inputs to UserControl Machines")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgWin.inputs(Run)})
        .push_to    (out.m_tasks)
        .args       ({      idScnParts,                       idSigValFloat,                    idSigUpdFloat,                               idUserInput,                     idVhControls,           idDeltaTimeIn})
        .func([] (ACtxParts& rScnParts, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat, input::UserInputHandler const &rUserInput, VehicleTestControls &rVhControls, float const deltaTimeIn) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];

        // Control selected UsrCtrl machine

        float const thrRate = deltaTimeIn;
        float const thrChange =
                  float(rUserInput.button_state(rVhControls.m_btnThrMore).m_held) * thrRate
                - float(rUserInput.button_state(rVhControls.m_btnThrLess).m_held) * thrRate
                + float(rUserInput.button_state(rVhControls.m_btnThrMax).m_held)
                - float(rUserInput.button_state(rVhControls.m_btnThrMin).m_held);

        Vector3 const attitude
        {
              float(rUserInput.button_state(rVhControls.m_btnPitchDn).m_held)
            - float(rUserInput.button_state(rVhControls.m_btnPitchUp).m_held),
              float(rUserInput.button_state(rVhControls.m_btnYawLf).m_held)
            - float(rUserInput.button_state(rVhControls.m_btnYawRt).m_held),
              float(rUserInput.button_state(rVhControls.m_btnRollRt).m_held)
            - float(rUserInput.button_state(rVhControls.m_btnRollLf).m_held)
        };

        PerMachType &rUsrCtrl    = rScnParts.m_machines.m_perType[gc_mtUserCtrl];
        MachAnyId const mach = rUsrCtrl.m_localToAny[rVhControls.m_selectedUsrCtrl];
        lgrn::Span<NodeId const> const portSpan = rFloatNodes.m_machToNode[mach];

        auto const write_control = [&rSigValFloat, &rSigUpdFloat, portSpan] (PortEntry const& entry, float write, bool replace = true, float min = 0.0f, float max = 1.0f)
        {
            NodeId const node = connected_node(portSpan, entry.m_port);
            if (node == lgrn::id_null<NodeId>())
            {
                return; // not connected
            }

            float const oldVal = rSigValFloat[node];
            float const newVal = replace ? write : Magnum::Math::clamp(oldVal + write, min, max);

            if (oldVal != newVal)
            {
                rSigUpdFloat.assign(node, newVal);
            }
        };

        write_control(ports_userctrl::gc_throttleOut,   thrChange, false);
        write_control(ports_userctrl::gc_pitchOut,      attitude.x());
        write_control(ports_userctrl::gc_yawOut,        attitude.y());
        write_control(ports_userctrl::gc_rollOut,       attitude.z());

    });*/

    return out;
} // setup_vehicle_control

Session setup_camera_vehicle(
        TopTaskBuilder&             rBuilder,
        [[maybe_unused]] ArrayView<entt::any> const topData,
        Session const&              windowApp,
        Session const&              scene,
        Session const&              sceneRenderer,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              parts,
        Session const&              cameraCtrl,
        Session const&              vehicleCtrl)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,    TESTAPP_DATA_CAMERA_CTRL);
    OSP_DECLARE_GET_DATA_IDS(vehicleCtrl,   TESTAPP_DATA_VEHICLE_CONTROL);

    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();
    auto const tgVhCtrl = vehicleCtrl   .get_pipelines<PlVehicleCtrl>();
    auto const tgCS     = commonScene   .get_pipelines<PlCommonScene>();
    auto const tgPhys   = physics       .get_pipelines<PlPhysics>();

    Session out;

    /*
     * vehicle camera needs transforms and modifies cam-controller
     * shape spawner needs cam-controller, modifies transforms
     *
     *, tgPhys.physUpdate(Done)
     */

    rBuilder.task()
        .name       ("Update vehicle camera")
        .run_on     ({tgWin.sync(Run)})
        .sync_with  ({tgCmCt.camCtrl(Modify), tgPhys.physUpdate(Done) /*, tgCS.transform(Modify)*/})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,           idDeltaTimeIn,                 idBasic,                     idVhControls,                 idScnParts})
        .func([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn, ACtxBasic const& rBasic, VehicleTestControls& rVhControls, ACtxParts const& rScnParts) noexcept
    {
        if (MachLocalId const selectedLocal = rVhControls.m_selectedUsrCtrl;
            selectedLocal != lgrn::id_null<MachLocalId>())
        {
            // Follow selected UserControl machine

            // Obtain associated ActiveEnt
            // MachLocalId -> MachAnyId -> PartId -> RigidGroup -> ActiveEnt
            PerMachType const&  rUsrCtrls       = rScnParts.m_machines.m_perType.at(adera::gc_mtUserCtrl);
            MachAnyId const     selectedMach    = rUsrCtrls.m_localToAny        .at(selectedLocal);
            PartId const        selectedPart    = rScnParts.m_machineToPart     .at(selectedMach);
            WeldId const        weld            = rScnParts.m_partToWeld        .at(selectedPart);
            ActiveEnt const     selectedEnt     = rScnParts.weldToActive        .at(weld);

            if (rBasic.m_transform.contains(selectedEnt))
            {
                rCamCtrl.m_target = rBasic.m_transform.get(selectedEnt).m_transform.translation();
            }
        }
        else
        {
            // Free cam when no vehicle selected
            SysCameraController::update_move(
                    rCamCtrl,
                    deltaTimeIn, true);
        }

        SysCameraController::update_view(rCamCtrl, deltaTimeIn);
    });

    return out;
} // setup_camera_vehicle


} // namespace testapp::scenes
