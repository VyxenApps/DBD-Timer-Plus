#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <memory>
#include <functional>

struct RenderContext
{
	ID2D1RenderTarget* target = nullptr;
	ID2D1Factory* factory = nullptr;
	IDWriteFactory* dwriteFactory = nullptr;
	float width = 0.0f;
	float height = 0.0f;
	void* userData = nullptr;
};

class RenderNode
{
public:
	virtual ~RenderNode() = default;
	virtual void render(const RenderContext& ctx) = 0;
	virtual void release() {}
};

class LambdaNode : public RenderNode
{
	std::function<void(const RenderContext&)> fn_;
public:
	explicit LambdaNode(std::function<void(const RenderContext&)> fn)
		: fn_(std::move(fn)) {}

	void render(const RenderContext& ctx) override
	{
		if (fn_) fn_(ctx);
	}
};

class RenderPipeline
{
	std::vector<std::unique_ptr<RenderNode>> nodes_;
public:
	RenderPipeline() = default;
	RenderPipeline(const RenderPipeline&) = delete;
	RenderPipeline& operator=(const RenderPipeline&) = delete;

	void add(std::unique_ptr<RenderNode> node)
	{
		nodes_.push_back(std::move(node));
	}

	void addLambda(std::function<void(const RenderContext&)> fn)
	{
		nodes_.push_back(std::make_unique<LambdaNode>(std::move(fn)));
	}

	void execute(const RenderContext& ctx)
	{
		for (auto& node : nodes_)
		{
			if (node)
				node->render(ctx);
		}
	}

	void clear()
	{
		for (auto& node : nodes_)
		{
			if (node)
				node->release();
		}
		nodes_.clear();
	}

	size_t count() const { return nodes_.size(); }
	bool empty() const { return nodes_.empty(); }
};
