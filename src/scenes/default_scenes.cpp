#include "scene_factories.hpp"

#include <epochengine/particle/scenes.hpp>
#include <epochengine/particle/text.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace epochengine::particle
{
    namespace
    {
        class ShowcaseScene final : public IScene
        {
        public:
            explicit ShowcaseScene(std::unique_ptr<IScene> scene)
                : scene_(std::move(scene))
            {
            }

            [[nodiscard]] SceneInfo info() const noexcept override
            {
                return scene_->info();
            }

            void reset(const SceneResetContext& context) override
            {
                scene_->reset(context);
            }

            void update(const SceneUpdateContext& context) override
            {
                scene_->update(context);
            }

            void render(RenderFrame& frame, Bounds bounds) const override
            {
                scene_->render(frame, bounds);

                const SceneInfo scene_info = scene_->info();
                const float card_width = std::min(390.0F, std::max(280.0F, bounds.width * 0.34F));
                frame.rounded_rectangle(
                    { card_width * 0.5F + 12.0F, 34.0F },
                    { card_width * 0.5F, 26.0F },
                    8.0F,
                    { 0.018F, 0.028F, 0.045F, 0.88F },
                    150);
                frame.text(
                    { 24.0F, 16.0F },
                    scene_info.name,
                    TextSize{ 14.0F, 1.0F },
                    { 0.92F, 0.96F, 1.0F, 1.0F },
                    160);
                frame.text(
                    { 24.0F, 39.0F },
                    "LMB / RMB interact   R reset   SPACE pause   TAB next",
                    TextSize{ 10.0F, 1.0F },
                    { 0.52F, 0.64F, 0.76F, 0.96F },
                    160);
            }

            void pointer(const PointerEvent& event) override
            {
                scene_->pointer(event);
            }

            void resize(Bounds old_bounds, Bounds new_bounds) override
            {
                scene_->resize(old_bounds, new_bounds);
            }

            [[nodiscard]] SceneStats stats() const noexcept override
            {
                return scene_->stats();
            }

            [[nodiscard]] std::uint64_t state_hash() const noexcept override
            {
                return scene_->state_hash();
            }

            [[nodiscard]] std::string scene_document() const override
            {
                return scene_->scene_document();
            }

            bool apply_scene_document(std::string_view document) override
            {
                return scene_->apply_scene_document(document);
            }

        private:
            std::unique_ptr<IScene> scene_;
        };

        [[nodiscard]] std::unique_ptr<IScene> showcase(std::unique_ptr<IScene> scene)
        {
            return std::make_unique<ShowcaseScene>(std::move(scene));
        }
    }

    std::vector<std::unique_ptr<IScene>> make_default_scenes()
    {
        std::vector<std::unique_ptr<IScene>> result;
        result.reserve(15);
        result.push_back(scenes::make_particle_studio_scene());
        result.push_back(showcase(scenes::make_deterministic_fountain_scene()));
        result.push_back(showcase(scenes::make_flow_field_scene()));
        result.push_back(showcase(scenes::make_particle_life_scene()));
        result.push_back(showcase(scenes::make_cellular_automata_scene()));
        result.push_back(showcase(scenes::make_reaction_diffusion_scene()));
        result.push_back(showcase(scenes::make_hybrid_sand_scene()));
        result.push_back(showcase(scenes::make_fire_smoke_scene()));
        result.push_back(showcase(scenes::make_fireworks_scene()));
        result.push_back(showcase(scenes::make_galaxy_scene()));
        result.push_back(showcase(scenes::make_boids_scene()));
        result.push_back(showcase(scenes::make_sph_fluid_scene()));
        result.push_back(showcase(scenes::make_spring_cloth_scene()));
        result.push_back(showcase(scenes::make_physarum_scene()));
        result.push_back(showcase(scenes::make_weather_scene()));
        return result;
    }
}
