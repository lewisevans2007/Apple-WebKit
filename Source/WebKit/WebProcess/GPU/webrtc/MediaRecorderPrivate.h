/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "GPUProcessConnection.h"
#include "MediaRecorderIdentifier.h"
#include "SharedCARingBuffer.h"
#include "SharedVideoFrame.h"
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/MediaRecorderPrivate.h>
#include <wtf/MediaTime.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
}

namespace WebCore {
class MediaStreamPrivate;
class WebAudioBufferList;
}

namespace WebKit {

class MediaRecorderPrivate final
    : public WebCore::MediaRecorderPrivate
    , public CanMakeWeakPtr<MediaRecorderPrivate> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<MediaRecorderPrivate> create(WebCore::MediaStreamPrivate&, const WebCore::MediaRecorderPrivateOptions&);
    ~MediaRecorderPrivate();

private:
    MediaRecorderPrivate(WebCore::MediaStreamPrivate&, const WebCore::MediaRecorderPrivateOptions&);

    // WebCore::MediaRecorderPrivate
    void videoFrameAvailable(WebCore::VideoFrame&, WebCore::VideoFrameTimeMetadata) final;
    void fetchData(CompletionHandler<void(RefPtr<WebCore::FragmentedSharedBuffer>&&, const String& mimeType, double)>&&) final;
    void stopRecording(CompletionHandler<void()>&&) final;
    void startRecording(StartRecordingCallback&&) final;
    void audioSamplesAvailable(const WTF::MediaTime&, const WebCore::PlatformAudioData&, const WebCore::AudioStreamDescription&, size_t) final;
    const String& mimeType() const final;
    void pauseRecording(CompletionHandler<void()>&&) final;
    void resumeRecording(CompletionHandler<void()>&&) final;

    void gpuProcessConnectionDidClose();

    class GPUProcessDidCloseObserver final
        : public GPUProcessConnection::Client
        , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GPUProcessDidCloseObserver> {
    public:
        static Ref<GPUProcessDidCloseObserver> create(MediaRecorderPrivate& recorder) { return adoptRef(*new GPUProcessDidCloseObserver(recorder)); }

        // GPUProcessConnection::Client
        void ref() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GPUProcessDidCloseObserver>::ref(); }
        void deref() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GPUProcessDidCloseObserver>::deref(); }
        ThreadSafeWeakPtrControlBlock& controlBlock() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GPUProcessDidCloseObserver>::controlBlock(); }

    private:
        explicit GPUProcessDidCloseObserver(MediaRecorderPrivate& recorder)
            : m_recorder(recorder)
        {
        }

        void gpuProcessConnectionDidClose(GPUProcessConnection&) final
        {
            callOnMainRunLoop([recorder = m_recorder] {
                if (recorder)
                    recorder->gpuProcessConnectionDidClose();
            });
        }

        WeakPtr<MediaRecorderPrivate> m_recorder;
    };

    MediaRecorderIdentifier m_identifier;
    Ref<WebCore::MediaStreamPrivate> m_stream;
    Ref<IPC::Connection> m_connection;

    std::unique_ptr<ProducerSharedCARingBuffer> m_ringBuffer;
    std::optional<WebCore::CAAudioStreamDescription> m_description;
    std::unique_ptr<WebCore::WebAudioBufferList> m_silenceAudioBuffer;
    uint64_t m_numberOfFrames { 0 };
    WebCore::MediaRecorderPrivateOptions m_options;
    bool m_hasVideo { false };
    bool m_isStopped { false };
    std::optional<WebCore::IntSize> m_blackFrameSize;

    Ref<GPUProcessDidCloseObserver> m_gpuProcessDidCloseObserver;
    SharedVideoFrameWriter m_sharedVideoFrameWriter;
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
