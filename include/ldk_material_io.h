/**
 * # Material serialization
 *
 * Read and write material descriptors using the engine asset and TML representations.
 */

#ifndef LDK_MATERIAL_IO_H
#define LDK_MATERIAL_IO_H

#include <ldk_material.h>
#include <module/ldk_asset_manager.h>
#include <stdx/stdx_tml.h>
#include <stdx/stdx_strbuilder.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Callback for reporting a material I/O diagnostic.
   * @param message Message displayed to the user.
   * @param user Opaque caller-supplied data passed to the callback.
   * @group ldk.asset.material
   */
  typedef void (*LDKMaterialDiagnosticFn)(const char *message, void *user);

  /** @group ldk.asset.material */
  typedef struct LDKMaterialIOContext
  {
    LDKAssetManager *assets;
    /* Additional diagnostic sink; missing images are also logged by the engine. */
    LDKMaterialDiagnosticFn diagnostic;
    void *user;
  } LDKMaterialIOContext;

  /** @group ldk.asset.material */
  typedef struct LDKMaterialIOResult
  {
    char error[512];
  } LDKMaterialIOResult;

  /* Read material fields from a TML node. Type is required; tint defaults to
   * white. Textured materials default to opaque alpha mode and a 0.5 cutout
   * threshold when the alpha fields are absent. Texture paths are logical
   * asset paths. Missing images use the procedural checker and emit a
   * recoverable diagnostic. out_desc is only assigned on success. result is
   * optional. */
  /**
   * @brief Read the material desc.
   * @param context Context used by this operation.
   * @param doc Doc used by this operation.
   * @param fields TML node containing serialized fields.
   * @param out_desc Receives the descriptor.
   * @param result Optional result structure populated with error details.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.asset.material
   */
  LDK_API bool ldk_material_desc_read(const LDKMaterialIOContext *context,
      const TMLDocument *doc, const TMLNode *fields,
      LDKMaterialDesc *out_desc, LDKMaterialIOResult *result);

  /* Append material fields; cutout alpha fields are emitted only when active.
   * indent counts two-space indentation levels. result is optional. Discard the
   * output if writing fails. */
  /**
   * @brief Write the material desc.
   * @param context Context used by this operation.
   * @param desc Description or configuration for this operation.
   * @param out Output object or buffer receiving the result.
   * @param indent Indentation depth used while serializing output.
   * @param result Optional result structure populated with error details.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.asset.material
   */
  LDK_API bool ldk_material_desc_write(const LDKMaterialIOContext *context,
      const LDKMaterialDesc *desc, XStrBuilder *out, u32 indent,
      LDKMaterialIOResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_MATERIAL_IO_H */
