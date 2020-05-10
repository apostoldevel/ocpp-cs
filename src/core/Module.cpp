/*++

Library name:

  apostol-core

Module Name:

  Module.cpp

Notices:

  Apostol Core

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#include "Core.hpp"
#include "Module.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <sstream>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Module {

        unsigned char random_char() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);
            return static_cast<unsigned char>(dis(gen));
        }
        //--------------------------------------------------------------------------------------------------------------

        CString GetUID(unsigned int len) {
            CString S(len, ' ');

            for (unsigned int i = 0; i < len / 2; i++) {
                unsigned char rc = random_char();
                ByteToHexStr(S.Data() + i * 2 * sizeof(unsigned char), S.Size(), &rc, 1);
            }

            return S;
        }
        //--------------------------------------------------------------------------------------------------------------

//      A0000-P0000-O0000-S0000-T0000-O0000-L00000
//      012345678901234567890123456789012345678901
//      0         1         2         3         4
        CString ApostolUID() {

            CString S(GetUID(APOSTOL_MODULE_UID_LENGTH));

            S[ 0] = 'A';
            S[ 5] = '-';
            S[ 6] = 'P';
            S[11] = '-';
            S[12] = 'O';
            S[17] = '-';
            S[18] = 'S';
            S[23] = '-';
            S[24] = 'T';
            S[29] = '-';
            S[30] = 'O';
            S[35] = '-';
            S[36] = 'L';

            return S;
        }
#ifdef WITH_POSTGRESQL
        //--------------------------------------------------------------------------------------------------------------

        //-- CJob ------------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CJob::CJob(CCollection *ACCollection) : CCollectionItem(ACCollection) {
            m_JobId = ApostolUID();
            m_PollQuery = nullptr;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CJobManager -----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CJob *CJobManager::Get(int Index) {
            return (CJob *) inherited::GetItem(Index);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CJobManager::Set(int Index, CJob *Value) {
            inherited::SetItem(Index, (CCollectionItem *) Value);
        }
        //--------------------------------------------------------------------------------------------------------------

        CJob *CJobManager::Add(CPQPollQuery *Query) {
            auto LJob = new CJob(this);
            LJob->PollQuery(Query);
            return LJob;
        }
        //--------------------------------------------------------------------------------------------------------------

        CJob *CJobManager::FindJobById(const CString &Id) {
            CJob *LJob = nullptr;
            for (int I = 0; I < Count(); ++I) {
                LJob = Get(I);
                if (LJob->JobId() == Id)
                    return LJob;
            }
            return nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        CJob *CJobManager::FindJobByQuery(CPQPollQuery *Query) {
            CJob *LJob = nullptr;
            for (int I = 0; I < Count(); ++I) {
                LJob = Get(I);
                if (LJob->PollQuery() == Query)
                    return LJob;
            }
            return nullptr;
        }
#endif
        //--------------------------------------------------------------------------------------------------------------

        //-- CApostolModule --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CApostolModule::CApostolModule(CModuleManager *AManager): CCollectionItem(AManager), CGlobalComponent() {
            m_pMethods = CStringList::Create(true);
            m_Headers.Add("Content-Type");
        }
        //--------------------------------------------------------------------------------------------------------------

        CApostolModule::~CApostolModule() {
            delete m_pMethods;
        }
        //--------------------------------------------------------------------------------------------------------------

        const CString &CApostolModule::GetAllowedMethods(CString &AllowedMethods) const {
            if (AllowedMethods.IsEmpty()) {
                if (m_pMethods->Count() > 0) {
                    CMethodHandler *Handler;
                    for (int i = 0; i < m_pMethods->Count(); ++i) {
                        Handler = (CMethodHandler *) m_pMethods->Objects(i);
                        if (Handler->Allow()) {
                            if (AllowedMethods.IsEmpty())
                                AllowedMethods = m_pMethods->Strings(i);
                            else
                                AllowedMethods += _T(", ") + m_pMethods->Strings(i);
                        }
                    }
                }
            }
            return AllowedMethods;
        }
        //--------------------------------------------------------------------------------------------------------------

        const CString &CApostolModule::GetAllowedHeaders(CString &AllowedHeaders) const {
            if (AllowedHeaders.IsEmpty()) {
                for (int i = 0; i < m_Headers.Count(); ++i) {
                    if (AllowedHeaders.IsEmpty())
                        AllowedHeaders = m_Headers.Strings(i);
                    else
                        AllowedHeaders += _T(", ") + m_Headers.Strings(i);
                }
            }
            return AllowedHeaders;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApostolModule::MethodNotAllowed(CHTTPServerConnection *AConnection) {
            auto LReply = AConnection->Reply();

            CReply::GetStockReply(LReply, CReply::not_allowed);

            if (!AllowedMethods().IsEmpty())
                LReply->AddHeader(_T("Allow"), AllowedMethods());

            AConnection->SendReply();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApostolModule::DoOptions(CHTTPServerConnection *AConnection) {
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            CReply::GetStockReply(LReply, CReply::no_content);

            if (!AllowedMethods().IsEmpty())
                LReply->AddHeader(_T("Allow"), AllowedMethods());

            AConnection->SendReply();
#ifdef _DEBUG
            if (LRequest->URI == _T("/quit"))
                GApplication->SignalProcess()->Quit();
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApostolModule::CORS(CHTTPServerConnection *AConnection) {
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            const CHeaders& LRequestHeaders = LRequest->Headers;
            CHeaders& LReplyHeaders = LReply->Headers;

            const CString& Origin = LRequestHeaders.Values("origin");
            if (!Origin.IsEmpty()) {
                LReplyHeaders.AddPair("Access-Control-Allow-Origin", Origin);
                LReplyHeaders.AddPair("Access-Control-Allow-Methods", AllowedMethods());
                LReplyHeaders.AddPair("Access-Control-Allow-Headers", AllowedHeaders());
            }
        }
        //--------------------------------------------------------------------------------------------------------------
#ifdef WITH_POSTGRESQL

        void CApostolModule::EnumQuery(CPQResult *APQResult, CQueryResult& AResult) {
            CStringList LFields;

            for (int I = 0; I < APQResult->nFields(); ++I) {
                LFields.Add(APQResult->fName(I));
            }

            for (int Row = 0; Row < APQResult->nTuples(); ++Row) {
                AResult.Add(CStringPairs());
                for (int Col = 0; Col < APQResult->nFields(); ++Col) {
                    if (APQResult->GetIsNull(Row, Col)) {
                        AResult.Last().AddPair(LFields[Col].c_str(), "");
                    } else {
                        if (APQResult->fFormat(Col) == 0) {
                            AResult.Last().AddPair(LFields[Col].c_str(), APQResult->GetValue(Row, Col));
                        } else {
                            AResult.Last().AddPair(LFields[Col].c_str(), "<binary>");
                        }
                    }
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApostolModule::QueryToResults(CPQPollQuery *APollQuery, CQueryResults& AResults) {
            CPQResult *LResult = nullptr;
            for (int i = 0; i < APollQuery->ResultCount(); ++i) {
                LResult = APollQuery->Results(i);
                if (LResult->ExecStatus() == PGRES_TUPLES_OK || LResult->ExecStatus() == PGRES_SINGLE_TUPLE) {
                    AResults.Add(CQueryResult());
                    EnumQuery(LResult, AResults[i]);
                } else {
                    throw Delphi::Exception::EDBError(LResult->GetErrorMessage());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CPQPollQuery *CApostolModule::GetQuery(CPollConnection *AConnection) {
            CPQPollQuery *LQuery = GApplication->GetQuery(AConnection);

            if (Assigned(LQuery)) {
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                LQuery->OnPollExecuted([this](auto && APollQuery) { DoPostgresQueryExecuted(APollQuery); });
                LQuery->OnException([this](auto && APollQuery, auto && AException) { DoPostgresQueryException(APollQuery, AException); });
#else
                LQuery->OnPollExecuted(std::bind(&CApostolModule::DoPostgresQueryExecuted, this, _1));
                LQuery->OnException(std::bind(&CApostolModule::DoPostgresQueryException, this, _1, _2));
#endif
            }

            return LQuery;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CApostolModule::ExecSQL(const CStringList &SQL, CPollConnection *AConnection,
                                     COnPQPollQueryExecutedEvent &&OnExecuted, COnPQPollQueryExceptionEvent &&OnException) {

            auto LQuery = GetQuery(AConnection);

            if (LQuery == nullptr)
                throw Delphi::Exception::Exception("ExecSQL: GetQuery() failed!");

            if (OnExecuted != nullptr)
                LQuery->OnPollExecuted(static_cast<COnPQPollQueryExecutedEvent &&>(OnExecuted));

            if (OnException != nullptr)
                LQuery->OnException(static_cast<COnPQPollQueryExceptionEvent &&>(OnException));

            LQuery->SQL() = SQL;

            if (LQuery->QueryStart() != POLL_QUERY_START_ERROR) {
                return true;
            } else {
                delete LQuery;
            }

            Log()->Error(APP_LOG_ALERT, 0, "ExecSQL: QueryStart() failed!");

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------
#endif
        CHTTPClient *CApostolModule::GetClient(const CString &Host, uint16_t Port) {
            return GApplication->GetClient(Host.c_str(), Port);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApostolModule::DebugRequest(CRequest *ARequest) {
            DebugMessage("[%p] Request:\n%s %s HTTP/%d.%d\n", ARequest, ARequest->Method.c_str(), ARequest->URI.c_str(), ARequest->VMajor, ARequest->VMinor);

            for (int i = 0; i < ARequest->Headers.Count(); i++)
                DebugMessage("%s: %s\n", ARequest->Headers[i].Name.c_str(), ARequest->Headers[i].Value.c_str());

            if (!ARequest->Content.IsEmpty())
                DebugMessage("\n%s\n", ARequest->Content.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApostolModule::DebugReply(CReply *AReply) {
            DebugMessage("[%p] Reply:\nHTTP/%d.%d %d %s\n", AReply, AReply->VMajor, AReply->VMinor, AReply->Status, AReply->StatusText.c_str());

            for (int i = 0; i < AReply->Headers.Count(); i++)
                DebugMessage("%s: %s\n", AReply->Headers[i].Name.c_str(), AReply->Headers[i].Value.c_str());

            if (!AReply->Content.IsEmpty())
                DebugMessage("\n%s\n", AReply->Content.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApostolModule::DebugConnection(CHTTPServerConnection *AConnection) {
            DebugMessage("\n[%p] [%s:%d] [%d] ", AConnection, AConnection->Socket()->Binding()->PeerIP(),
                         AConnection->Socket()->Binding()->PeerPort(), AConnection->Socket()->Binding()->Handle());

            DebugRequest(AConnection->Request());

            static auto OnReply = [](CObject *Sender) {
                auto LConnection = dynamic_cast<CHTTPServerConnection *> (Sender);
                auto LBinding = LConnection->Socket()->Binding();

                if (Assigned(LBinding)) {
                    DebugMessage("\n[%p] [%s:%d] [%d] ", LConnection, LBinding->PeerIP(),
                                 LBinding->PeerPort(), LBinding->Handle());
                }

                DebugReply(LConnection->Reply());
            };

            AConnection->OnReply(OnReply);
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CModuleManager --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CModuleManager::HeartbeatModules() {
            for (int i = 0; i < ModuleCount(); i++)
                Modules(i)->Heartbeat();
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CModuleManager::ExecuteModules(CTCPConnection *AConnection) {
            auto LConnection = dynamic_cast<CHTTPServerConnection *> (AConnection);

            CApostolModule *LModule = nullptr;

            auto LRequest = LConnection->Request();

            const CString &UserAgent = LRequest->Headers.Values(_T("user-agent"));

            int Index = 0;
            while (Index < ModuleCount() && !Modules(Index)->CheckUserAgent(UserAgent))
                Index++;

            if (Index == ModuleCount()) {
                LConnection->SendStockReply(CReply::forbidden);
                return false;
            }

            LModule = Modules(Index);

            DoBeforeExecuteModule(LModule);
            try {
                LModule->Execute(LConnection);
            } catch (...) {
                LConnection->SendStockReply(CReply::internal_server_error);
                DoAfterExecuteModule(LModule);
                throw;
            }
            DoAfterExecuteModule(LModule);

            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

    }
}
}
