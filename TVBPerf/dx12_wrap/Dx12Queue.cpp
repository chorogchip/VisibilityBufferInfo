#include "dx12_wrap/Dx12Queue.h"

#include <utility>

#include "util/Utils.h"

namespace dx12 {

	Dx12Queue::~Dx12Queue() {
		release();
	}

	Dx12Queue::Dx12Queue(Dx12Queue&& other) noexcept {
		move_from(std::move(other));
	}

	Dx12Queue& Dx12Queue::operator=(Dx12Queue&& other) noexcept {
		if (this != &other) {
			release();
			move_from(std::move(other));
		}

		return *this;
	}

	Dx12Queue Dx12Queue::create(
		const Dx12Device& device,
		D3D12_COMMAND_LIST_TYPE type) {

		Dx12Queue result;

		result.type_ = type;

		D3D12_COMMAND_QUEUE_DESC queue_desc{};
		queue_desc.Type = type;
		queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.NodeMask = 0;

		ID3D12Device* native_device = device.get();

		Utils::throw_if_failed(
			native_device->CreateCommandQueue(
				&queue_desc,
				IID_PPV_ARGS(result.queue_.ReleaseAndGetAddressOf())),
			"command queue create");

		Utils::throw_if_failed(
			native_device->CreateFence(
				0,
				D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(result.fence_.ReleaseAndGetAddressOf())),
			"fence create");

		result.fence_event_ = CreateEventW(
			nullptr,
			FALSE,
			FALSE,
			nullptr);

		if (!result.fence_event_) {
			Utils::throw_win32_lasterr("fence event create");
		}

		return result;
	}

	void Dx12Queue::execute(
		ID3D12CommandList* command_list) {

		ID3D12CommandList* command_lists[] = { command_list };
		this->execute(1, command_lists);

	}

	void Dx12Queue::execute(
		UINT command_list_count,
		ID3D12CommandList* const* command_lists) {

		if (!queue_) {
			throw std::runtime_error(
				"Dx12Queue::execute: command queue is null");
		}

		if (command_list_count > 0 && !command_lists) {
			throw std::invalid_argument(
				"Dx12Queue::execute: command_lists is null");
		}

		queue_->ExecuteCommandLists(
			command_list_count,
			command_lists);
	}

	UINT64 Dx12Queue::signal() {
		if (!queue_ || !fence_) {
			throw std::runtime_error(
				"Dx12Queue::signal: queue or fence is null");
		}

		const UINT64 value = ++fence_value_;

		Utils::throw_if_failed(
			queue_->Signal(fence_.Get(), value),
			"fence signal");

		return value;
	}

	void Dx12Queue::wait_for_value(UINT64 value) {
		if (!fence_ || !fence_event_) {
			throw std::runtime_error(
				"Dx12Queue::wait_for_value: fence is not initialized");
		}

		const UINT64 completed = fence_->GetCompletedValue();

		if (completed == UINT64_MAX) {
			throw std::runtime_error("Dx12Queue: device was removed");
		}

		if (completed >= value) {
			return;
		}

		Utils::throw_if_failed(
			fence_->SetEventOnCompletion(
				value,
				fence_event_),
			"fence set event");

		const DWORD result = WaitForSingleObjectEx(
			fence_event_,
			INFINITE,
			FALSE);

		if (result != WAIT_OBJECT_0) {
			Utils::throw_win32_lasterr("fence wait");
		}
	}

	void Dx12Queue::wait_for_gpu() {
		wait_for_value(signal());
	}

	UINT64 Dx12Queue::completed_value() const {
		if (!fence_) {
			return 0;
		}

		return fence_->GetCompletedValue();
	}

	void Dx12Queue::release() noexcept {
		if (fence_event_) {
			CloseHandle(fence_event_);
			fence_event_ = nullptr;
		}

		fence_.Reset();
		queue_.Reset();

		fence_value_ = 0;
	}

	void Dx12Queue::move_from(Dx12Queue&& other) noexcept {
		queue_ = std::move(other.queue_);
		fence_ = std::move(other.fence_);

		fence_value_ = other.fence_value_;
		fence_event_ = other.fence_event_;
		type_ = other.type_;

		other.fence_value_ = 0;
		other.fence_event_ = nullptr;
		other.type_ = D3D12_COMMAND_LIST_TYPE_DIRECT;
	}

} // namespace dx12