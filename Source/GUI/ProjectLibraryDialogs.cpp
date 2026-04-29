#include "ProjectLibraryDialogs.h"

#include <functional>

#include "../App/AppConfig.h"
#include "../App/AppContext.h"
#include "../App/MainComponent.h"
#include "../App/ProjectSession.h"
#include "../GUI/NameEntryModal.h"
#include "../GUI/RackViewComponent.h"
#include "../GUI/UnsavedChangesModal.h"
#include "../Storage/ForgeStoragePaths.h"
#include "../Storage/ProjectSerializer.h"
#include "../Utilities/Logger.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{
namespace
{

void finishSuccessfulProjectLoad(AppContext& appContext, const juce::File& f)
{
    if (appContext.appConfig != nullptr)
    {
        appContext.appConfig->setLastLoadedProjectPath(f.getFullPathName());
        appContext.appConfig->saveToFile();
    }

    if (appContext.mainComponent != nullptr)
        appContext.mainComponent->refreshProjectDependentViews();

    if (appContext.projectSession != nullptr)
        appContext.projectSession->clearProjectDirtyAfterSave();
}

void copyToLastSessionBackup(const juce::File& primarySavedFile)
{
    if (! primarySavedFile.existsAsFile())
        return;

    ensureForgeStorageFoldersExist();
    const juce::File dest = getBackupsDirectory().getChildFile("LastSession.forgeproject");

    if (! primarySavedFile.copyFileTo(dest))
        Logger::warn("FORGE7: could not write LastSession backup to " + dest.getFullPathName());
}

void showSimpleAlert(juce::Component* associatedComponent, const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                           title,
                                           message,
                                           "OK",
                                           associatedComponent,
                                           nullptr);
}

bool loadProjectFileIntoCurrentSession(juce::Component* modalParent,
                                      AppContext& appContext,
                                      const juce::File& f,
                                      std::function<void(const juce::String&)> statusMessage,
                                      std::function<void()> afterSuccessfulLoad)
{
    if (appContext.projectSerializer == nullptr || appContext.pluginHostManager == nullptr)
        return false;

    const auto r = appContext.projectSerializer->loadProjectFromFile(f,
                                                                       appContext.pluginHostManager,
                                                                       appContext.audioEngine);

    if (r.failed())
    {
        Logger::warn("FORGE7: load project failed - " + r.getErrorMessage());
        showSimpleAlert(modalParent,
                        "Load failed",
                        r.getErrorMessage().isNotEmpty() ? r.getErrorMessage() : juce::String("Unknown error."));
        return false;
    }

    finishSuccessfulProjectLoad(appContext, f);
    Logger::info("FORGE7: loaded project - " + f.getFullPathName());

    if (afterSuccessfulLoad != nullptr)
        afterSuccessfulLoad();

    if (statusMessage)
        statusMessage("Loaded: " + f.getFileNameWithoutExtension());

    return true;
}

} // namespace

void runSaveProjectToLibraryDialog(juce::Component* modalParent,
                                   AppContext& appContext,
                                   std::function<void(const juce::String&)> statusMessage,
                                   std::function<void()> onSavedSuccessfully)
{
    juce::ignoreUnused(modalParent);

    if (appContext.projectSerializer == nullptr || appContext.pluginHostManager == nullptr)
        return;

    ensureForgeStorageFoldersExist();

    const juce::String fromDisplay =
        appContext.getProjectDisplayName != nullptr ? appContext.getProjectDisplayName() : juce::String();
    const juce::String initial =
        fromDisplay.isNotEmpty() ? fromDisplay : juce::String("Untitled Project");

    NameEntryModal::showSaveDialog(
        appContext,
        "Save Project",
        initial,
        [&appContext, statusMessage](const juce::String& rawText,
                                     bool replaceIfExisting,
                                     juce::String& errorOut) -> NameEntrySaveOutcome
        {
            const juce::String entered = rawText.trim();
            const juce::String safeName = sanitizeLibraryItemName(entered);

            if (safeName.isEmpty())
            {
                errorOut = "Enter a valid name.";
                return NameEntrySaveOutcome::Failed;
            }

            const juce::File target =
                getProjectsDirectory().getChildFile(safeName + kForgeProjectExtension);

            if (target.existsAsFile() && ! replaceIfExisting)
                return NameEntrySaveOutcome::NeedReplace;

            if (appContext.setProjectDisplayName != nullptr)
                appContext.setProjectDisplayName(safeName);

            const auto r = appContext.projectSerializer->saveProjectToFile(target,
                                                                           appContext.pluginHostManager,
                                                                           appContext.audioEngine);

            if (r.failed())
            {
                Logger::warn("FORGE7: library save failed - " + r.getErrorMessage());
                errorOut = r.getErrorMessage().isNotEmpty() ? r.getErrorMessage()
                                                            : juce::String("Unknown error.");
                return NameEntrySaveOutcome::Failed;
            }

            if (appContext.projectSession != nullptr)
                appContext.projectSession->clearProjectDirtyAfterSave();

            copyToLastSessionBackup(target);

            if (appContext.appConfig != nullptr)
            {
                appContext.appConfig->setLastLoadedProjectPath(target.getFullPathName());
                appContext.appConfig->saveToFile();
            }

            Logger::info("FORGE7: saved project to library - " + target.getFullPathName());

            if (statusMessage)
                statusMessage("Saved: " + safeName);

            return NameEntrySaveOutcome::Success;
        },
        std::move(onSavedSuccessfully));
}

namespace
{

void openOrReplaceProjectFromFile(juce::Component* modalParent,
                                  AppContext& appContext,
                                  const juce::File& f,
                                  std::function<void(const juce::String&)> statusMessage,
                                  std::function<void()> afterSuccessfulLoad)
{
    if (appContext.projectSession != nullptr && appContext.projectSession->isProjectDirty())
    {
        UnsavedChangesModal::show(
            appContext,
            [modalParent, f, &appContext, statusMessage, afterSuccessfulLoad](UnsavedProjectChoice choice)
            {
                if (choice == UnsavedProjectChoice::Cancel)
                    return;

                if (choice == UnsavedProjectChoice::Discard)
                {
                    loadProjectFileIntoCurrentSession(modalParent,
                                                      appContext,
                                                      f,
                                                      statusMessage,
                                                      std::move(afterSuccessfulLoad));
                    return;
                }

                runSaveProjectToLibraryDialog(
                    modalParent,
                    appContext,
                    statusMessage,
                    [modalParent, f, &appContext, statusMessage, afterSuccessfulLoad]()
                    {
                        loadProjectFileIntoCurrentSession(modalParent,
                                                          appContext,
                                                          f,
                                                          statusMessage,
                                                          std::move(afterSuccessfulLoad));
                    });
            });
        return;
    }

    loadProjectFileIntoCurrentSession(modalParent,
                                      appContext,
                                      f,
                                      statusMessage,
                                      std::move(afterSuccessfulLoad));
}

} // namespace

void runLoadProjectFromLibraryBrowser(juce::Component* modalParent,
                                      AppContext& appContext,
                                      std::function<void(const juce::String&)> statusMessage)
{
    if (appContext.projectSerializer == nullptr || appContext.pluginHostManager == nullptr)
        return;

    const juce::Array<juce::File> files = listLibraryProjectFiles();

    if (files.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               "Load Project",
                                               "No saved projects found in the FORGE7 library yet.",
                                               "OK",
                                               modalParent,
                                               nullptr);
        return;
    }

    juce::StringArray choices;

    for (const auto& f : files)
        choices.add(f.getFileNameWithoutExtension());

    juce::AlertWindow w("Load Project", "Select a project:", juce::MessageBoxIconType::QuestionIcon, modalParent);
    w.addComboBox("projectPick", choices, "Saved projects:");
    w.addButton("Load", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (w.runModalLoop() != 1)
        return;

    if (auto* cb = w.getComboBoxComponent("projectPick"))
    {
        const int idx = cb->getSelectedItemIndex();

        if (! juce::isPositiveAndBelow(idx, files.size()))
            return;

        const juce::File f = files.getReference(idx);
        openOrReplaceProjectFromFile(modalParent, appContext, f, std::move(statusMessage), {});
    }
}

void runExportProjectWithFileChooser(juce::Component* modalParent, AppContext& appContext)
{
    juce::ignoreUnused(modalParent);

    if (appContext.projectSerializer == nullptr || appContext.pluginHostManager == nullptr)
        return;

    AppContext* ctx = &appContext;

    const juce::String suggestName =
        ctx->getProjectDisplayName != nullptr ? ctx->getProjectDisplayName() : juce::String("Project");

    auto chooser = std::make_shared<juce::FileChooser>("Export FORGE 7 project",
                                                        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                            .getChildFile(sanitizeLibraryItemName(suggestName)
                                                                          + kForgeProjectExtension),
                                                        "*.forgeproject;*.forge7.json");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [chooser, ctx](const juce::FileChooser& fc)
                         {
                             const juce::File f = fc.getResult();

                             if (f == juce::File {} || ctx == nullptr || ctx->projectSerializer == nullptr)
                                 return;

                             const auto r = ctx->projectSerializer->saveProjectToFile(f,
                                                                                        ctx->pluginHostManager,
                                                                                        ctx->audioEngine);

                             if (r.failed())
                                 Logger::warn("FORGE7: export failed - " + r.getErrorMessage());
                             else
                             {
                                 if (ctx->projectSession != nullptr)
                                     ctx->projectSession->clearProjectDirtyAfterSave();

                                 Logger::info("FORGE7: exported project - " + f.getFullPathName());
                             }
                         });
}

void runImportProjectWithFileChooser(juce::Component* modalParent, AppContext& appContext)
{
    juce::ignoreUnused(modalParent);

    if (appContext.projectSerializer == nullptr || appContext.pluginHostManager == nullptr)
        return;

    AppContext* ctx = &appContext;

    auto chooser = std::make_shared<juce::FileChooser>("Import FORGE 7 project",
                                                       juce::File {},
                                                       "*.forgeproject;*.forge7.json");

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [chooser, ctx, modalParent](const juce::FileChooser& fc)
                         {
                             const juce::File f = fc.getResult();

                             if (f == juce::File {} || ctx == nullptr || ctx->projectSerializer == nullptr)
                                 return;

                             openOrReplaceProjectFromFile(modalParent, *ctx, f, {}, {});
                         });
}

void openLibraryProjectFileReplacingCurrent(juce::Component* modalParent,
                                            AppContext& appContext,
                                            const juce::File& projectFile,
                                            const int selectSceneIndexAfterLoad,
                                            const int selectChainIndexAfterLoad)
{
    const int si = selectSceneIndexAfterLoad;
    const int ci = selectChainIndexAfterLoad;

    openOrReplaceProjectFromFile(
        modalParent,
        appContext,
        projectFile,
        {},
        [&appContext, si, ci]()
        {
            if (appContext.projectSession == nullptr)
                return;

            if (si >= 0)
                appContext.projectSession->switchToScene(si);

            if (ci >= 0)
                appContext.projectSession->switchToChain(ci);
        });
}

} // namespace forge7
