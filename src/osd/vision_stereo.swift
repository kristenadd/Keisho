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
//
// The same RealityKit view tracks PlayStation VR2 Sense controllers for the
// light-gun titles: an anchor follows each controller's aim point, and its
// ray is cast onto the window's surface (SpatialAim below).

import ARKit
import GameController
import Metal
import RealityKit
import SwiftUI
import UIKit

@MainActor
final class StereoModel: ObservableObject {
    let root  = Entity()
    let aim   = SpatialAim()
    private var crosshairs: [Entity] = []
    private var crosshairColours: [SIMD3<Float>] = []
    private var windowSize = SIMD2<Float>(0, 0)
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
        aim.windowSize = size
        windowSize     = size
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

    /// Light-gun crosshairs over the stereo picture: `values` holds 6 floats
    /// a mark (x, y as 0..1 of the window, radius as a fraction of its height,
    /// then red, green, blue). Children of `root`, so they hide with the layer.
    func setCrosshairs(_ values: UnsafePointer<Float>, count: Int) {
        while crosshairs.count < count {
            let mark = Entity()
            root.addChild(mark)
            crosshairs.append(mark)
            crosshairColours.append(SIMD3<Float>(-1, -1, -1))
        }
        for (i, mark) in crosshairs.enumerated() {
            guard i < count, windowSize.x > 0, windowSize.y > 0 else {
                mark.isEnabled = false
                continue
            }
            let v      = { (k: Int) in values[i * 6 + k] }
            let colour = SIMD3<Float>(v(3), v(4), v(5))
            let radius = v(2) * windowSize.y
            if colour != crosshairColours[i] || mark.children.isEmpty
                || abs((mark.components[CrosshairSize.self]?.radius ?? 0) - radius) > 1e-5 {
                buildCrosshair(mark, radius: radius, colour: colour)
                crosshairColours[i] = colour
            }
            // Just in front of the picture, at the screen's depth.
            mark.position  = SIMD3<Float>((v(0) - 0.5) * windowSize.x, (0.5 - v(1)) * windowSize.y, 0.004)
            mark.isEnabled = true
        }
    }

    /// The built-in crosshair's four ticks (the flat one also has a ring).
    private func buildCrosshair(_ mark: Entity, radius: Float, colour: SIMD3<Float>) {
        mark.children.removeAll()
        mark.components.set(CrosshairSize(radius: radius))
        let material = UnlitMaterial(color: UIColor(red: CGFloat(colour.x), green: CGFloat(colour.y),
                                                    blue: CGFloat(colour.z), alpha: 1))
        let length = radius * 1.2
        let width  = max(radius * 0.14, 0.0012)
        let centre = radius * 1.0
        for (dx, dy) in [(-1, 0), (1, 0), (0, -1), (0, 1)] as [(Float, Float)] {
            let size = dx != 0 ? SIMD3<Float>(length, width, 0.0005) : SIMD3<Float>(width, length, 0.0005)
            let bar  = ModelEntity(mesh: .generateBox(size: size), materials: [material])
            bar.position = SIMD3<Float>(dx * centre, dy * centre, 0)
            mark.addChild(bar)
        }
        let dot = ModelEntity(mesh: .generateBox(size: SIMD3<Float>(width, width, 0.0005)), materials: [material])
        mark.addChild(dot)
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

struct CrosshairSize: Component {
    var radius: Float
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
                content.add(model.aim.root)
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

/// Unbuffered, so it shows in `devicectl ... launch --console` as it happens.
private func aimLog(_ text: String) {
    FileHandle.standardError.write(Data("[INFO] vision aim: \(text)\n".utf8))
}

/// PlayStation VR2 Sense controllers (or any spatial controller) aimed at the
/// window. Each gets an AnchorEntity on its "aim" location; the point where the
/// aim's forward ray (-Z) crosses the window's surface (z = 0 in the view, its
/// centre at the origin) is the gun's position on the window.
@MainActor
final class SpatialAim {
    let root = Entity()
    var windowSize = SIMD2<Float>(0, 0)  // metres, from StereoModel.layout

    private struct Hand {
        var anchor: AnchorEntity?
        var chirality: Int32 = 0  // 0 unknown, 1 left, 2 right
    }
    /// GCController is not Sendable; the loader only reads it to find the accessory.
    private struct Device: @unchecked Sendable {
        let controller: GCController
    }
    private nonisolated static func source(for device: Device) async throws
        -> AnchoringComponent.AccessoryAnchoringSource {
        try await AnchoringComponent.AccessoryAnchoringSource(device: device.controller)
    }

    private var hands: [ObjectIdentifier: Hand] = [:]
    private var controllers: [ObjectIdentifier: GCController] = [:]
    private var session: SpatialTrackingSession?
    private var started = false
    private static let pitchCos = Float(cos(22.0 * Double.pi / 180.0))
    private static let pitchSin = Float(sin(22.0 * Double.pi / 180.0))

    func start() {
        guard !started else { return }
        started = true
        for name in [NSNotification.Name.GCControllerDidConnect, .GCControllerDidDisconnect] {
            NotificationCenter.default.addObserver(forName: name, object: nil, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated { self?.refresh() }
            }
        }
        NotificationCenter.default.addObserver(forName: UIScene.didActivateNotification, object: nil,
                                               queue: .main) { [weak self] _ in
            MainActor.assumeIsolated { self?.runTracking(attempt: 0) }
        }
        refresh()
        runTracking(attempt: 0)
    }

    /// Ask for accessory tracking, then run the session that reveals the
    /// anchors' transforms. Without it they all sit at the origin. Tried again
    /// for a while if the system says no, which it does before the window is
    /// focused or while the permission prompt is up.
    private var trackingLive = false
    private var trackingBusy = false
    private func runTracking(attempt: Int) {
        guard !trackingLive, !trackingBusy else { return }
        trackingBusy = true
        Task { @MainActor in
            let arkit  = ARKitSession()
            let status = await arkit.requestAuthorization(for: [.accessoryTracking])
            aimLog("accessory tracking permission: \(status[.accessoryTracking].map { "\($0)" } ?? "?")")
            let session = self.session ?? SpatialTrackingSession()
            self.session = session
            let unavailable = await session.run(SpatialTrackingSession.Configuration(tracking: [.accessory]))
            self.trackingBusy = false
            if let unavailable, unavailable.anchor.contains(.accessory) {
                aimLog("accessory tracking unavailable (attempt \(attempt + 1))")
                if attempt < 20 {
                    try? await Task.sleep(for: .seconds(3))
                    self.runTracking(attempt: attempt + 1)
                }
                return
            }
            self.trackingLive = true
            aimLog("tracking session running")
        }
    }

    private func refresh() {
        let spatial = GCController.controllers().filter { $0.productCategory == GCProductCategorySpatialController }
        let live    = Set(spatial.map { ObjectIdentifier($0) })
        for (id, hand) in hands where !live.contains(id) {
            hand.anchor?.removeFromParent()
            hands[id]       = nil
            controllers[id] = nil
        }
        for controller in spatial {
            let id = ObjectIdentifier(controller)
            guard hands[id] == nil else { continue }
            hands[id]       = Hand()
            controllers[id] = controller
            let device = Device(controller: controller)
            Task { @MainActor in
                do {
                    let source = try await SpatialAim.source(for: device)
                    guard let location = source.locationName(named: "aim"), self.hands[id] != nil else { return }
                    let anchor = AnchorEntity(.accessory(from: source, location: location), trackingMode: .predicted)
                    self.root.addChild(anchor)
                    var chirality: Int32 = 0
                    switch source.underlyingAccessory?.inherentChirality {
                    case .left?:  chirality = 1
                    case .right?: chirality = 2
                    default:      chirality = 0
                    }
                    self.hands[id]?.anchor    = anchor
                    self.hands[id]?.chirality = chirality
                } catch {
                    aimLog("could not track a controller: \(error)")
                }
            }
        }
    }

    /// Fill up to `max` entries: the controller (unretained), its hand, whether
    /// it is tracked and pointing at the window's plane, and where on the window
    /// (0..1, outside that is off the window). Returns how many were written.
    func sample(controllers out: UnsafeMutablePointer<UnsafeRawPointer?>,
                values: UnsafeMutablePointer<Float>, max: Int) -> Int {
        var count = 0
        for (id, hand) in hands {
            guard count < max, let controller = controllers[id] else { continue }
            var tracked: Float = 0
            var x: Float       = -1
            var y: Float       = -1
            if let anchor = hand.anchor, anchor.isAnchored, windowSize.x > 0, windowSize.y > 0 {
                let m      = anchor.transformMatrix(relativeTo: root)
                let origin = SIMD3<Float>(m.columns.3.x, m.columns.3.y, m.columns.3.z)
                // The aim location's -Z sits about 22° above where a Sense
                // controller is pointed as a gun (measured on the headset), so
                // tip it down about the controller's own X axis.
                let back   = simd_normalize(SIMD3<Float>(m.columns.2.x, m.columns.2.y, m.columns.2.z))
                let up     = simd_normalize(SIMD3<Float>(m.columns.1.x, m.columns.1.y, m.columns.1.z))
                let ahead  = -back * SpatialAim.pitchCos - up * SpatialAim.pitchSin
                if ahead.z < -0.05, origin.z > 0 {  // pointing in towards the window
                    let t   = -origin.z / ahead.z
                    let hit = origin + ahead * t
                    x       = 0.5 + hit.x / windowSize.x
                    y       = 0.5 - hit.y / windowSize.y
                    tracked = 1
                }
            }
            out[count]              = UnsafeRawPointer(Unmanaged.passUnretained(controller).toOpaque())
            values[count * 4 + 0]   = Float(hand.chirality)
            values[count * 4 + 1]   = tracked
            values[count * 4 + 2]   = x
            values[count * 4 + 3]   = y
            count += 1
        }
        return count
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
            // UIKit's Swift name for this differs between SDKs: bringSubview(toFront:) in
            // visionOS 26 (Xcode 26), bringSubviewToFront(_:) in visionOS 27. The Objective-C
            // selector is the same in both, so call that.
            _ = parent.perform(NSSelectorFromString("bringSubviewToFront:"), with: host.view)
        }
    }
}

@_cdecl("sm2_vision_crosshairs")
func sm2_vision_crosshairs(_ window: UnsafeMutableRawPointer?, _ values: UnsafePointer<Float>?, _ count: Int32) {
    let windowAddress = UInt(bitPattern: window)
    let valueAddress  = UInt(bitPattern: values)
    MainActor.assumeIsolated {
        guard let window = UnsafeMutableRawPointer(bitPattern: windowAddress),
              let values = UnsafePointer<Float>(bitPattern: valueAddress) else { return }
        let ui = Unmanaged<UIWindow>.fromOpaque(window).takeUnretainedValue()
        StereoLayer.attach(to: ui).setCrosshairs(values, count: Int(count))
    }
}

@_cdecl("sm2_vision_aim_poll")
func sm2_vision_aim_poll(_ window: UnsafeMutableRawPointer?, _ controllers: UnsafeMutablePointer<UnsafeRawPointer?>?,
                         _ values: UnsafeMutablePointer<Float>?, _ max: Int32) -> Int32 {
    // Called on the main thread, once a frame of a light-gun title while a
    // spatial controller is connected.
    let windowAddress = UInt(bitPattern: window)
    let outAddress    = UInt(bitPattern: controllers)
    let valueAddress  = UInt(bitPattern: values)
    return MainActor.assumeIsolated {
        guard let window = UnsafeMutableRawPointer(bitPattern: windowAddress),
              let out = UnsafeMutablePointer<UnsafeRawPointer?>(bitPattern: outAddress),
              let values = UnsafeMutablePointer<Float>(bitPattern: valueAddress) else { return 0 }
        let ui    = Unmanaged<UIWindow>.fromOpaque(window).takeUnretainedValue()
        let model = StereoLayer.attach(to: ui)
        model.aim.start()
        return Int32(model.aim.sample(controllers: out, values: values, max: Int(max)))
    }
}
