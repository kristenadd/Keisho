// ██╗  ██╗███████╗██╗███████╗██╗  ██╗ ██████╗
// ██║ ██╔╝██╔════╝██║██╔════╝██║  ██║██╔═══██╗
// █████╔╝ █████╗  ██║███████╗███████║██║   ██║
// ██╔═██╗ ██╔══╝  ██║╚════██║██╔══██║██║   ██║
// ██║  ██╗███████╗██║███████║██║  ██║╚██████╔╝
// ╚═╝  ╚═╝╚══════╝╚═╝╚══════╝╚═╝  ╚═╝ ╚═════╝
//            継 承  ( k e i s h ō )
//
// A Sega Model 1, 2 & 3 arcade emulator.
// Copyright (c) 2025+ Daniel Martin (dmanlfc)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This header must not be removed. Keisho is licensed under the GNU General
// Public License, version 3 or later, with additional terms under section 7 of
// that licence. See LICENSE and NOTICE.
//
// Apple Vision Pro's stereo picture. A window's own layer is one flat image, so
// the stereo frame is shown by RealityKit instead: a plane over the window
// whose material (data/visionos/StereoMaterial.usda) samples the left half of
// the side-by-side frame for the left eye and the right half for the right.
// The frame reaches it through a LowLevelTexture, copied on the GPU from the
// image Vulkan drew (render/vk/stereo_out.cpp). The layer ignores touches, so a
// tap still reaches the game and opens the settings, which are drawn flat on
// the window beneath once the layer hides.

import Metal
import RealityKit
import SwiftUI
import UIKit

@MainActor
final class StereoModel: ObservableObject {
    let root  = Entity()
    let plane = ModelEntity(mesh: .generatePlane(width: 1, height: 1))

    @Published var aspect: Float = 4.0 / 3.0

    private var material: ShaderGraphMaterial?
    private var texture: LowLevelTexture?
    private var resource: TextureResource?
    private var queue: MTLCommandQueue?
    private var loading = false

    init() {
        root.addChild(plane)
        root.isEnabled = false
    }

    private var meshSize = SIMD3<Float>(0, 0, 0)  // width, height, corner radius

    /// Fit the plane inside the window, `size` in metres, at the picture's
    /// aspect, centred (the view's origin) just in front of its surface, with
    /// the window's rounded corners (`cornerRadius`, metres) so it does not
    /// show past them.
    func layout(_ size: SIMD2<Float>, cornerRadius: Float) {
        guard size.x > 0, size.y > 0 else { return }
        var width  = size.x
        var height = aspect > 0 ? width / aspect : size.y  // 0: Stretch fills the window
        if height > size.y {
            height = size.y
            width  = height * aspect
        }
        // Built at its true size rather than scaled, so the corners stay round.
        let radius = min(cornerRadius, min(width, height) / 2)
        let wanted = SIMD3<Float>(width, height, radius)
        if wanted != meshSize {
            plane.model?.mesh = .generatePlane(width: width, height: height, cornerRadius: radius)
            meshSize = wanted
        }
        plane.position = SIMD3<Float>(0, 0, 0.001)
    }

    func show(_ source: MTLTexture?, width: Int, height: Int, visible: Bool) {
        guard visible, let source else {
            root.isEnabled = false
            return
        }
        loadMaterial()
        guard let material, ensureTexture(width: width, height: height, material: material),
              let texture else {
            return
        }
        if queue == nil || queue!.device !== source.device {
            queue = source.device.makeCommandQueue()
        }
        // Copied now and waited for: Vulkan reuses the source once this frame
        // is submitted, and RealityKit reads the copy whenever it draws.
        guard let commands = queue?.makeCommandBuffer() else { return }
        let target = texture.replace(using: commands)
        guard let blit = commands.makeBlitCommandEncoder() else { return }
        blit.copy(from: source, to: target)
        blit.endEncoding()
        commands.commit()
        commands.waitUntilCompleted()
        root.isEnabled = true
    }

    private func loadMaterial() {
        guard material == nil, !loading else { return }
        loading = true
        Task { @MainActor in
            do {
                material = try await ShaderGraphMaterial(named: "/Root/StereoMaterial",
                                                         from: "StereoMaterial.usda", in: Bundle.main)
            } catch {
                print("Keisho: the stereo material did not load: \(error)")
            }
            loading = false
        }
    }

    private func ensureTexture(width: Int, height: Int, material: ShaderGraphMaterial) -> Bool {
        if let texture, texture.descriptor.width == width, texture.descriptor.height == height {
            return true
        }
        var descriptor              = LowLevelTexture.Descriptor()
        descriptor.textureType      = .type2D
        descriptor.pixelFormat      = .rgba8Unorm_srgb
        descriptor.width            = width
        descriptor.height           = height
        descriptor.depth            = 1
        descriptor.mipmapLevelCount = 1
        descriptor.textureUsage     = [.shaderRead]
        do {
            let texture  = try LowLevelTexture(descriptor: descriptor)
            let resource = try TextureResource(from: texture)
            var updated  = material
            try updated.setParameter(name: "StereoTexture", value: .textureResource(resource))
            plane.model?.materials = [updated]
            self.texture  = texture
            self.resource = resource
            return true
        } catch {
            print("Keisho: the stereo texture could not be made: \(error)")
            return false
        }
    }
}

struct StereoView: View {
    @ObservedObject var model: StereoModel
    @Environment(\.physicalMetrics) private var metrics

    /// A visionOS window's corner radius, in points. The system does not
    /// report it; this matches the glass window's outline.
    static let windowCornerRadius: CGFloat = 46

    var body: some View {
        GeometryReader { proxy in
            RealityView { content in
                content.add(model.root)
            } update: { _ in
                _ = model.aspect  // relayout when the picture's shape changes
                model.layout(SIMD2<Float>(Float(metrics.convert(proxy.size.width, to: .meters)),
                                          Float(metrics.convert(proxy.size.height, to: .meters))),
                             cornerRadius: Float(metrics.convert(StereoView.windowCornerRadius, to: .meters)))
            }
        }
        .allowsHitTesting(false)
    }
}

@MainActor
private enum StereoLayer {
    static var model: StereoModel?
    static var host: UIHostingController<StereoView>?

    static func attach(to window: UIWindow) -> StereoModel {
        if let model, let host, host.view.window === window {
            return model
        }
        let model = StereoModel()
        let host  = UIHostingController(rootView: StereoView(model: model))
        host.view.backgroundColor        = .clear
        host.view.isUserInteractionEnabled = false
        host.view.frame                  = window.bounds
        host.view.autoresizingMask       = [.flexibleWidth, .flexibleHeight]
        window.addSubview(host.view)
        Self.model = model
        Self.host  = host
        return model
    }
}

@_cdecl("sm2_vision_stereo_present")
func sm2_vision_stereo_present(_ window: UnsafeMutableRawPointer?, _ texture: UnsafeMutableRawPointer?,
                               _ width: UInt32, _ height: UInt32, _ show: Bool, _ aspect: Float) {
    // Called on the main thread (the emulator's loop runs there under UIKit).
    // The pointers cross into the main actor as addresses.
    let windowAddress  = UInt(bitPattern: window)
    let textureAddress = UInt(bitPattern: texture)
    MainActor.assumeIsolated {
        guard let window = UnsafeMutableRawPointer(bitPattern: windowAddress) else { return }
        let ui    = Unmanaged<UIWindow>.fromOpaque(window).takeUnretainedValue()
        let model = StereoLayer.attach(to: ui)
        if model.aspect != aspect {
            model.aspect = aspect
        }
        var source: MTLTexture?
        if let texture = UnsafeMutableRawPointer(bitPattern: textureAddress) {
            source = Unmanaged<AnyObject>.fromOpaque(texture).takeUnretainedValue() as? MTLTexture
        }
        model.show(source, width: Int(width), height: Int(height), visible: show)
        if let host = StereoLayer.host, let parent = host.view.superview {
            parent.bringSubviewToFront(host.view)
        }
    }
}
