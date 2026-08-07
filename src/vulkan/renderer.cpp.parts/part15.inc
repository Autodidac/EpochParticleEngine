    Renderer::~Renderer() = default;
    Renderer::Renderer(Renderer&&) noexcept = default;
    Renderer& Renderer::operator=(Renderer&&) noexcept = default;

    std::expected<void, RendererError> Renderer::draw(const RenderFrame& frame)
    {
        if (!impl_)
        {
            return std::unexpected(make_error(
                RendererErrorCode::rendering_failed,
                "Vulkan renderer is in a moved-from state"));
        }
        return impl_->draw(frame);
    }

    void Renderer::wait_idle() noexcept
    {
        if (impl_)
            impl_->wait_idle();
    }

    const std::string& Renderer::device_name() const noexcept
    {
        static const std::string unavailable{ "Unavailable" };
        return impl_ ? impl_->device_name() : unavailable;
    }

    std::size_t Renderer::item_capacity() const noexcept
    {
        return impl_ ? impl_->item_capacity() : 0;
    }

    std::size_t Renderer::maximum_item_capacity() const noexcept
    {
        return impl_ ? impl_->maximum_item_capacity() : 0;
    }

    bool Renderer::validation_enabled() const noexcept
    {
        return impl_ && impl_->validation_enabled();
    }

    bool Renderer::vertical_sync() const noexcept
    {
        return impl_ && impl_->vertical_sync();
    }

    Bounds Renderer::drawable_bounds() const noexcept
    {
        if (!impl_)
            return { 0.0F, 0.0F };
        const detail::NativeExtent extent = detail::query_extent(impl_->surface_source());
        return {
            static_cast<float>(extent.width),
            static_cast<float>(extent.height)
        };
    }
}
