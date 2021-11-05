import * as React from 'react'

import { getLocale } from '../../../../../../common/locale'
import { NavButton } from '../../../../extension'

// Styled Components
import { DisclaimerText, InfoIcon } from '../style'
import {
  ConnectingButton,
  ConnectingButtonText,
  HardwareButton,
  HardwareButtonRow,
  HardwareInfoColumn,
  HardwareInfoRow,
  HardwareTitle,
  LedgerIcon,
  TrezorIcon,
  ErrorText,
  LoadIcon
} from './style'

// Custom types
import { HardwareWalletAccount, HardwareWalletConnectOpts, LedgerDerivationPaths, ErrorMessage } from './types'

import {
  kLedgerHardwareVendor,
  kTrezorHardwareVendor
} from '../../../../../constants/types'

import HardwareWalletAccountsList from './accounts-list'

export interface Props {
  onConnectHardwareWallet: (opts: HardwareWalletConnectOpts) => Promise<HardwareWalletAccount[]>
  onAddHardwareAccounts: (selected: HardwareWalletAccount[]) => void
  getBalance: (address: string) => Promise<string>
}

const derivationBatch = 4

export default function (props: Props) {
  const [selectedHardwareWallet, setSelectedHardwareWallet] = React.useState<string>(kLedgerHardwareVendor)
  const [isConnecting, setIsConnecting] = React.useState<boolean>(false)
  const [isConnected, setIsConnected] = React.useState<boolean>(false)
  const [accounts, setAccounts] = React.useState<Array<HardwareWalletAccount>>([])
  const [selectedDerivationPaths, setSelectedDerivationPaths] = React.useState<string[]>([])
  const [connectionError, setConnectionError] = React.useState<ErrorMessage | undefined>(undefined)
  const [selectedDerivationScheme, setSelectedDerivationScheme] = React.useState<string>(
    LedgerDerivationPaths.LedgerLive.toString()
  )

  const getErrorMessage = (error: any) => {
    if (error.statusCode && error.statusCode === 27404) {  // Unknown Error
      return { error: getLocale('braveWalletConnectHardwareInfo2'), userHint: '' }
    }

    if (error.statusCode && (error.statusCode === 27904 || error.statusCode === 26368)) {  // INCORRECT_LENGTH or INS_NOT_SUPPORTED
      return { error: error.message, userHint: getLocale('braveWalletConnectHardwareWrongApplicationUserHint') }
    }

    if (!error || !error.message) {
      return { error: getLocale('braveWalletUnknownInternalError'), userHint: '' }
    }

    return { error: error.message, userHint: '' }
  }

  const onSelectedDerivationScheme = (scheme: string) => {
    setSelectedDerivationScheme(scheme)
    setAccounts([])
    props.onConnectHardwareWallet({
      hardware: selectedHardwareWallet,
      startIndex: 0,
      stopIndex: derivationBatch,
      scheme: scheme
    }).then((result) => {
      setAccounts(result)
      setIsConnected(true)
    }).catch((error) => {
      setIsConnecting(false)
      setConnectionError(getErrorMessage(error))
    })
  }

  const onConnectHardwareWallet = (hardware: string) => {
    setIsConnecting(true)
    props.onConnectHardwareWallet({
      hardware,
      startIndex: accounts.length,
      stopIndex: accounts.length + derivationBatch,
      scheme: selectedDerivationScheme
    }).then((result) => {
      setAccounts([...accounts, ...result])
      setIsConnected(true)
      setIsConnecting(false)
    }).catch((error) => {
      setConnectionError(getErrorMessage(error))
      setIsConnected(false)
      setIsConnecting(false)
    })
  }

  const onAddAccounts = () => {
    const selectedAccounts = accounts.filter(o => selectedDerivationPaths.includes(o.derivationPath))
    props.onAddHardwareAccounts(selectedAccounts)
  }

  const getBalance = (address: string) => {
    return props.getBalance(address)
  }

  const onSelectLedger = () => {
    setSelectedHardwareWallet(kLedgerHardwareVendor)
  }

  const onSelectTrezor = () => {
    setSelectedHardwareWallet(kTrezorHardwareVendor)
  }

  const onSubmit = () => onConnectHardwareWallet(selectedHardwareWallet)

  if (isConnected) {
    return (
      <HardwareWalletAccountsList
        hardwareWallet={selectedHardwareWallet}
        accounts={accounts}
        onLoadMore={onSubmit}
        selectedDerivationPaths={selectedDerivationPaths}
        setSelectedDerivationPaths={setSelectedDerivationPaths}
        selectedDerivationScheme={selectedDerivationScheme}
        setSelectedDerivationScheme={onSelectedDerivationScheme}
        onAddAccounts={onAddAccounts}
        getBalance={getBalance}
      />
    )
  }

  return (
    <>
      <HardwareTitle>{getLocale('braveWalletConnectHardwareTitle')}</HardwareTitle>
      <HardwareButtonRow>
        <HardwareButton onClick={onSelectLedger} isSelected={selectedHardwareWallet === kLedgerHardwareVendor}>
          <LedgerIcon />
        </HardwareButton>
        <HardwareButton onClick={onSelectTrezor} isSelected={selectedHardwareWallet === kTrezorHardwareVendor}>
          <TrezorIcon />
        </HardwareButton>
      </HardwareButtonRow>
      <HardwareInfoRow>
        <InfoIcon />
        <HardwareInfoColumn>
          <DisclaimerText>
            {getLocale('braveWalletConnectHardwareInfo1').replace('$1', selectedHardwareWallet)}
          </DisclaimerText>
          <DisclaimerText>{getLocale('braveWalletConnectHardwareInfo2')}</DisclaimerText>
        </HardwareInfoColumn>
      </HardwareInfoRow>
      {connectionError &&
        <>
          <ErrorText>{connectionError.error}</ErrorText>
          <ErrorText>{connectionError.userHint}</ErrorText>
        </>
      }

      {isConnecting ? (
        <ConnectingButton>
          <LoadIcon size='small' />
          <ConnectingButtonText>{getLocale('braveWalletConnectingHardwareWallet')}</ConnectingButtonText>
        </ConnectingButton>
      ) : (
        <NavButton onSubmit={onSubmit} text={getLocale('braveWalletAddAccountConnect')} buttonType='primary' />
      )}
    </>
  )
}
