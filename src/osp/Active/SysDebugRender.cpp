#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

#include "SysDebugRender.h"
#include "ActiveScene.h"


using namespace osp;

// for _1, _2, _3, ... std::bind arguments
using namespace std::placeholders;

// for the 0xrrggbb_rgbf and _deg literals
using namespace Magnum::Math::Literals;


SysDebugRender::SysDebugRender(ActiveScene &rScene) :
        m_scene(rScene),
        m_renderDebugDraw(rScene.get_render_order(), "debug", "", "",
                          std::bind(&SysDebugRender::draw, this, _1))
{

}

void SysDebugRender::draw(CompCamera const& camera)
{
    using Magnum::GL::Renderer;

    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::enable(Renderer::Feature::FaceCulling);


    auto drawGroup = m_scene.get_registry().group<CompDrawableDebug>(
                            entt::get<CompTransform>);

    Matrix4 entRelative;

    for(auto entity: drawGroup)
    {
        CompDrawableDebug& drawable = drawGroup.get<CompDrawableDebug>(entity);
        CompTransform& transform = drawGroup.get<CompTransform>(entity);

        entRelative = camera.m_inverse * transform.m_transformWorld;

        (*drawable.m_shader)
                .setDiffuseColor(drawable.m_color)
                .setAmbientColor(0x111111_rgbf)
                .setSpecularColor(0x330000_rgbf)
                .setLightPosition({10.0f, 15.0f, 5.0f})
                .setTransformationMatrix(entRelative)
                .setProjectionMatrix(camera.m_projection)
                .setNormalMatrix(entRelative.normalMatrix())
                .draw(*(drawable.m_mesh));

        //std::cout << "render! \n";
    }
}
