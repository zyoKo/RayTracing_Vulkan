#include "vk/command_buffer.h"

#include "vk_check.h"

namespace rtrt
{

void CommandBuffer::Begin()
{
    VkCommandBufferBeginInfo BeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    RTRT_CHECK_VK(vkBeginCommandBuffer(Handle, &BeginInfo));
}

void CommandBuffer::End()
{
    RTRT_CHECK_VK(vkEndCommandBuffer(Handle));
}

}  // namespace rtrt
